#include "proposer.hh"

Status ProposerService::Prepare(ServerContext* context, const PrepareRequest* request, PrepareResponse* response) {
    std::lock_guard<std::mutex> guard(m_mutex);
    std::vector<ClientContext> client_contexts(acceptor_stubs.size());
    std::vector<Status> statuses(acceptor_stubs.size());
    int i = 0;
    for (auto& stub : acceptor_stubs) {
        statuses[i] = stub.second->Prepare(&client_contexts[i], *request, response);
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
	    stub.second->Accept(&client_contexts[i], accept_request, &accept_response);
	}
    }

    return Status::OK;
}

