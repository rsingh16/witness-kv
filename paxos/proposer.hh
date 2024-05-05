#ifndef __proposer_hh__
#define __proposer_hh__

#include "paxos.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ServerContext;
using grpc::Status;

using paxos::Acceptor;
using paxos::Proposer;
using paxos::PrepareRequest;
using paxos::PrepareResponse;
using paxos::AcceptRequest;
using paxos::AcceptResponse;

class ProposerService final : public Proposer::Service {
private:
    int majority_threshold;
    std::unordered_map<int64_t, std::vector<PrepareResponse>> prepare_responses;
    std::map<int, std::unique_ptr<Acceptor::Stub>> acceptor_stubs;
    // TODO: FIXME - For now we can use a terminal mutex.
    std::mutex m_mutex;

public:
    ProposerService(int num_acceptors, const std::map<int, std::string>& acceptor_addresses)
        : majority_threshold(num_acceptors / 2 + 1) {
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(INT_MAX);
        for (const auto& address : acceptor_addresses) {
            auto channel = grpc::CreateCustomChannel(address.second, grpc::InsecureChannelCredentials(), args);
            acceptor_stubs[address.first] = Acceptor::NewStub(channel);
        }
    }
    Status Prepare(ServerContext* context, const PrepareRequest* request, PrepareResponse* response) override;
};
#endif // __proposer_hh__
