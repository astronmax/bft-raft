#include "in_memory_state_mgr.hxx"
#include "bft_state_machine.hpp"
#include "request_handler.hpp"

#include <libnuraft/nuraft.hxx>
#include <crow.h>

#include <fstream>

using namespace nuraft;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: ./bft-raft <ID> <RPC PORT> <HTTP PORT>" << std::endl;
        return 0;
    }

    auto server_id = std::atoi(argv[1]);
    auto server_rpc_port = std::atoi(argv[2]);
    auto server_http_port = std::atoi(argv[3]);

    auto srv_state_machine = cs_new<bft_raft::bft_state_machine>();
    auto srv_state_mgr = cs_new<inmem_state_mgr>(server_id, "localhost:" + std::string(argv[2]));

    std::ifstream cluster_config_file("cluster_config.json");
    if (!cluster_config_file.is_open()) {
        std::cerr << "Can't open cluster_config.json" << std::endl;
        return 1;
    }

    std::string json_str((std::istreambuf_iterator<char>(cluster_config_file)), 
                          std::istreambuf_iterator<char>());
    cluster_config_file.close();

    auto json_data = crow::json::load(json_str);
    if (!json_data) {
        std::cerr << "JSON error" << std::endl;
        return 1;
    }

    cluster_config cluster_cfg {};
    for (const auto& node : json_data) {
        cluster_cfg.get_servers().push_back(cs_new<srv_config>(node["id"].i(), node["rpc_ep"].s()));
        srv_state_machine->add_http_endpoint(node["id"].i(), node["http_ep"].s());
    }
    srv_state_mgr->save_config(cluster_cfg);

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
        server_rpc_port,
        asio_opt,
        params
    );

    while (!server->is_initialized()) {
        std::this_thread::sleep_for( std::chrono::milliseconds(100) );
    }

    bft_raft::request_handler handler {server_http_port, server, srv_state_machine};
    handler.run();

    return 0;
}
