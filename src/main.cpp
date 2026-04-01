#include "in_memory_state_mgr.hxx"
#include "bft_state_machine.hpp"
#include "libnuraft/cluster_config.hxx"
#include "request_handler.hpp"

#include <libnuraft/nuraft.hxx>
#include <crow.h>
#include <cpr/cpr.h>
#include <CLI/CLI.hpp>

using namespace nuraft;

int main(int argc, char* argv[]) {
    CLI::App app;

    int server_id {};
    int rpc_port {};
    int http_port {};
    std::string connection_str {};
    std::string config_path {};

    app.add_option("--id", server_id, "Raft server identifier")->required(true);
    app.add_option("--rpc-port", rpc_port, "Raft server RPC port")->required(true);
    app.add_option("--http-port", http_port, "Server HTTP port for REST API")->required(true);
    app.add_option("--connect", connection_str, "HTTP endpoint from cluster");
    app.add_option("--config", config_path, "JSON file with seed nodes");

    CLI11_PARSE(app, argc, argv);

    auto server_rpc_ep = "localhost:" + std::to_string(rpc_port);
    auto server_http_ep = "localhost:" + std::to_string(http_port);

    auto srv_state_mgr = cs_new<inmem_state_mgr>(server_id, server_rpc_ep);

    auto srv_state_machine = cs_new<bft_raft::bft_state_machine>();
    srv_state_machine->add_http_endpoint(server_id, server_http_ep);

    if (!config_path.empty()) {
        std::ifstream file(config_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();
        auto json = crow::json::load(json_str);

        cluster_config cluster_cfg;
        for (const auto& ep : json) {
            auto server_cfg = cs_new<srv_config>(int32_t(ep["id"].i()), ep["rpc_ep"].s());
            cluster_cfg.get_servers().push_back(server_cfg);
            srv_state_machine->add_http_endpoint(int32_t(ep["id"].i()), ep["http_ep"].s());
        }
        srv_state_mgr->save_config(cluster_cfg);
    }

    asio_service::options asio_opt {};
    raft_params params {};
    params.with_election_timeout_lower(1000)
      .with_election_timeout_upper(2000)
      .with_hb_interval(500)
      .with_max_append_size(100);

    raft_launcher launcher {};
    auto server = launcher.init(
        srv_state_machine,
        srv_state_mgr,
        cs_new<logger>(),
        rpc_port,
        asio_opt,
        params
    );

    while (!server->is_initialized()) {
        std::this_thread::sleep_for( std::chrono::milliseconds(100) );
    }

    if (!connection_str.empty()) {
        auto response = cpr::Get(cpr::Url{"http://" + connection_str + "/get_leader"});
        auto resp_json = crow::json::load(response.text);
        std::string leader_addr = resp_json["http_ep"].s();
        
        crow::json::wvalue req;
        req["id"] = server_id;
        req["rpc_ep"] = server_rpc_ep;
        req["http_ep"] = server_http_ep;

        response = cpr::Post(
            cpr::Url{"http://" + leader_addr + "/register_node"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{req.dump()}
        );
        auto response_json = crow::json::load(response.text);
        auto new_http_endpoints = response_json["http_endpoints"];
        for (const auto& kv : new_http_endpoints) {
            srv_state_machine->add_http_endpoint(std::stoi(kv.key()), kv.s());
        }
    }

    bft_raft::request_handler handler {http_port, server, srv_state_machine};
    handler.run();

    return 0;
}
