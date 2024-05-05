#include "acceptor.hh"

Status 
AcceptorService::Prepare(ServerContext* context, const PrepareRequest* request, PrepareResponse* response) {
    std::lock_guard<std::mutex> guard(m_mutex);
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

Status
AcceptorService::Accept(ServerContext* context, const AcceptRequest* request, AcceptResponse* response) {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (request->proposal_number() >= min_proposal_number) {
	    accepted_proposal_number = request->proposal_number();
	    accepted_value = request->value();
	    response->set_accepted(true);
    }
    return Status::OK;

}
