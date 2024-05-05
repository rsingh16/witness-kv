#ifndef __acceptor_hh__
#define __acceptor_hh__

#include "paxos.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using grpc::ServerContext;
using grpc::Status;

using paxos::Acceptor;
using paxos::PrepareRequest;
using paxos::PrepareResponse;
using paxos::AcceptRequest;
using paxos::AcceptResponse;

class AcceptorService final : public Acceptor::Service {
private:
    int64_t min_proposal_number = 0;
    int64_t accepted_proposal_number = 0;
    std::string accepted_value;
    // TODO: FIXME - For now we can use a terminal mutex.
    std::mutex m_mutex;

public:
    AcceptorService() : min_proposal_number{0}, m_mutex{} {    
    }
    ~AcceptorService() = default;

    Status Prepare(ServerContext* context, const PrepareRequest* request, PrepareResponse* response) override;
    Status Accept(ServerContext* context, const AcceptRequest* request, AcceptResponse* response) override;
};

#endif // __acceptor_hh__
