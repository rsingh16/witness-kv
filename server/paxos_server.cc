#include <grpcpp/grpcpp.h>
#include "paxos.grpc.pb.h"
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <map>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using namespace paxos;

class AcceptorImpl final : public Acceptor::Service {
private:
    int64 min_proposal_number = 0;
    int64 accepted_proposal_number = 0;
    std::string accepted_value;

public:
    Status ReceivePrepare(ServerContext* context, const PrepareRequest* request, PrepareResponse* response) override {
        if (request->proposal_number() > min_proposal_number) {
            min_proposal_number = request->proposal_number();
            response->set_promise(true);
            response->set_proposal_number(request->proposal_number());
            if (accepted_proposal_number > 0) {
                response->set_accepted_value(accepted_value);
                response->set_accepted_proposal_number(accepted_proposal_number);
            }
        }
        return Status::OK;
    }

    Status SendAccept(ServerContext* context, const AcceptRequest* request, AcceptResponse* response) override {
        if (request->proposal_number() >= min_proposal_number) {
            accepted_proposal_number = request->proposal_number();
            accepted_value = request->value();
            response->set_accepted(true);
        }
        return Status::OK;
    }
};

class ProposerImpl final : public Proposer::Service {
private:
    int majority_threshold;
    std::unordered_map<int64, std::vector<PrepareResponse>> prepare_responses;
    std::map<int, std::unique_ptr<Acceptor::Stub>> acceptor_stubs;

public:
    ProposerImpl(int num_acceptors, const std::map<int, std::string>& acceptor_addresses)
        : majority_threshold(num_acceptors / 2 + 1) {
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(INT_MAX);
        for (const auto& address : acceptor_addresses) {
            auto channel = grpc::CreateCustomChannel(address.second, grpc::InsecureChannelCredentials(), args);
            acceptor_stubs[address.first] = Acceptor::NewStub(channel);
        }
    }

    Status Prepare(ServerContext* context, const PrepareRequest* request, PrepareResponse* response) override {
        std::vector<ClientContext> client_contexts(acceptor_stubs.size());
        std::vector<Status> statuses(acceptor_stubs.size());
        int i = 0;

        for (auto& stub : acceptor_stubs) {
            statuses[i] = stub.second->ReceivePrepare(&client_contexts[i], *request, response);
            if (statuses[i].ok() && response->promise()) {
                prepare_responses[request->proposal_number()].push_back(*response);
            }
            i++;
        }

        if (prepare_responses[request->proposal_number()].size() >= majority_threshold) {
            AcceptRequest accept_request;
            accept_request.set_proposal_number(request->proposal_number());
            accept_request.set_value("new_value"); 
            AcceptResponse accept_response;
            for (auto& stub : acceptor_stubs) {
                stub.second->SendAccept(&client_contexts[i], accept_request, &accept_response);
            }
        }

        return Status::OK;
    }
};

class LeaderImpl final : public Leader::Service {
private:
    int current_leader_id;
    int my_id;

public:
    LeaderImpl(int my_id) : my_id(my_id), current_leader_id(-1) {}

    void set_leader_id(int id) {
        current_leader_id = id;
    }

    Status Propose(ServerContext* context, const ProposeRequest* request, ProposeResponse* response) override {
        if (my_id == current_leader_id) {
            std::cout << "Leader " << my_id << " accepted proposal: " << request->value() << std::endl;
            response->set_success(true);
        } else {
            std::cout << "Node " << my_id << " rejected proposal as it is not the leader." << std::endl;
            response->set_success(false);
        }
        return Status::OK;
    }
};

class LeaderElectionImpl final : public LeaderElection::Service {
private:
    int my_id;
    std::map<int, std::unique_ptr<LeaderElection::Stub>> peers;
    LeaderImpl* leader_service;

public:
    LeaderElectionImpl(int id, const std::map<int, std::string>& peer_addresses, LeaderImpl* leader_service)
        : my_id(id), leader_service(leader_service) {
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(INT_MAX);
        for (const auto& peer : peer_addresses) {
            if (peer.first != my_id) {  // Do not create a stub for this node itself
                peers[peer.first] = LeaderElection::NewStub(
                    grpc::CreateCustomChannel(peer.second, grpc::InsecureChannelCredentials(), args));
            }
        }
    }

    Status Election(ServerContext* context, const paxos::ElectionMessage* request, paxos::ElectionResponse* response) {
        std::cout << "Node " << my_id << " received election message from node " << request->node_id() << std::endl;
        if (request->node_id() > my_id) {
            response->set_ack(true);
        } else {
            response->set_ack(false);
            if (peers.empty()) {
                // No higher nodes, declare itself as leader
                leader_service->set_leader_id(my_id);
                std::cout << "Node " << my_id << " declares itself as leader." << std::endl;
            }
        }
        return Status::OK;
    }

    void initiate_election() {
        bool higher_exists = false;
        for (const auto& peer : peers) {
            if (peer.first > my_id) {
		paxos::ElectionMessage msg;
                msg.set_node_id(my_id);
		paxos::ElectionResponse resp;
                grpc::ClientContext context;

                grpc::Status status = peer.second->Election(&context, msg, &resp);
                if (status.ok() && resp.ack()) {
                    higher_exists = true;
                    std::cout << "Higher node " << peer.first << " acknowledged election." << std::endl;
                    break;
                }
            }
        }

        if (!higher_exists) {
            // No higher nodes responded, declare itself as leader
            leader_service->set_leader_id(my_id);
            std::cout << "Node " << my_id << " is now the leader after election." << std::endl;
        }
    }
};
