#pragma once

#include "bft_state_machine.hpp"

#include <crow.h>
#include <libnuraft/nuraft.hxx>

#include <memory>

namespace bft_raft {

enum class response_status {
    OK,
    ERROR,
};

struct get_leader_resp {
    int id;
    std::string rpc_endpoint;
    std::string http_endpoint;
};

class request_handler final {
public:
    request_handler(int port,
                    nuraft::ptr<nuraft::raft_server> raft_srv,
                    std::shared_ptr<bft_raft::bft_state_machine> state_machine);
    ~request_handler() = default;

    get_leader_resp get_leader();
    response_status append_data(const std::string& msg);
    response_status register_node(const int id, const std::string& rpc_ep, const std::string& http_ep);

    void run();

private:
    nuraft::ptr<nuraft::raft_server> _raft_server;
    std::shared_ptr<bft_raft::bft_state_machine> _state_machine;
    int _port;

    crow::SimpleApp _app;
};

};
