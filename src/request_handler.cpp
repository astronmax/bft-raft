#include "request_handler.hpp"
#include "crow/common.h"
#include "crow/json.h"
#include "libnuraft/srv_config.hxx"

using namespace bft_raft;
using namespace nuraft;

struct get_response_status {
    static constexpr std::string operator[](response_status status) noexcept {
        switch (status) {
        case response_status::OK: return "OK";
        case response_status::ERROR: return "ERROR";
        }
        return "";
    }
};

request_handler::request_handler(int port, ptr<raft_server> raft_srv, std::shared_ptr<bft_state_machine> state_machine)
    : _port(port)
    , _raft_server(raft_srv)
    , _state_machine(state_machine)
{
    CROW_ROUTE(_app, "/get_leader").methods(crow::HTTPMethod::GET)
    ([this]() {
        crow::json::wvalue resp;
        auto resp_struct = this->get_leader();
        resp["status"] = get_response_status{}[response_status::OK];
        resp["id"] = resp_struct.id;
        resp["rpc_ep"] = resp_struct.rpc_endpoint;
        resp["http_ep"] = resp_struct.http_endpoint;

        return crow::response(resp);
    });

    CROW_ROUTE(_app, "/append_data").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        auto body_string = req.body;
        crow::json::wvalue resp;
        resp["status"] = get_response_status{}[this->append_data(body_string)];
        return crow::response(resp);
    });

    CROW_ROUTE(_app, "/register_node").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        auto body_json = crow::json::load(req.body);
        auto status = this->register_node(
            body_json["id"].i(),
            body_json["rpc_ep"].s(),
            body_json["http_ep"].s()
        );
        crow::json::wvalue resp;
        resp["status"] = get_response_status{}[status];
        return crow::response(resp);
    });
}

get_leader_resp request_handler::get_leader() {
    auto leader_id = _raft_server->get_leader();
    auto rpc_endpoint =  _raft_server->get_config()->get_server(leader_id)->get_endpoint();
    auto http_endpoint = _state_machine->get_http_endpoint(leader_id);

    return {leader_id, rpc_endpoint, http_endpoint};
}

response_status request_handler::append_data(const std::string& msg) {
    ptr<buffer> log = buffer::alloc(sizeof(int) + msg.size());
    buffer_serializer bs_log(log);
    bs_log.put_str(msg);
    _raft_server->append_entries({log});

    return response_status::OK;
}

response_status request_handler::register_node(const int id, const std::string& rpc_ep, const std::string& http_ep) {
    srv_config cfg {id, rpc_ep};
    _raft_server->add_srv(cfg);
    _state_machine->add_http_endpoint(id, http_ep);

    return response_status::OK;
}

void request_handler::run() {
    _app.port(_port).multithreaded().run();
}
