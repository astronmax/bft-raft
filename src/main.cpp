#include "in_memory_state_mgr.hxx"
#include "bft_state_machine.hpp"
#include "libnuraft/cluster_config.hxx"
#include "request_handler.hpp"

#include <libnuraft/nuraft.hxx>
#include <crow.h>
#include <boost/program_options.hpp>
#include <cpr/cpr.h>

namespace po = boost::program_options;
using namespace nuraft;

int main(int argc, char* argv[]) {
    po::options_description desc {"Allowed options"};
    desc.add_options()
        ("id", po::value<int>()->required(), "Raft server ID")
        ("rpc-port", po::value<int>()->required(), "Raft server RPC port")
        ("http-port", po::value<int>()->required(), "Server HTTP port for REST API")
        ("connect", po::value<std::string>(), "HTTP endpoint used to connect to the cluster")
        ("config", po::value<std::string>(), "JSON file with seed nodes");
    
    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cout << desc << std::endl;
        return 0;
    }

    auto server_id = vm["id"].as<int>();
    auto server_rpc_ep = "localhost:" + std::to_string(vm["rpc-port"].as<int>());
    auto server_http_ep = "localhost:" + std::to_string(vm["http-port"].as<int>());

    auto srv_state_mgr = cs_new<inmem_state_mgr>(server_id, server_rpc_ep);

    auto srv_state_machine = cs_new<bft_raft::bft_state_machine>();
    srv_state_machine->add_http_endpoint(server_id, server_http_ep);

    if (vm.contains("config")) {
        std::ifstream file(vm["config"].as<std::string>());
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
        vm["rpc-port"].as<int>(),
        asio_opt,
        params
    );

    while (!server->is_initialized()) {
        std::this_thread::sleep_for( std::chrono::milliseconds(100) );
    }

    if (vm.contains("connect")) {
        auto node_addr = vm["connect"].as<std::string>();
        auto response = cpr::Get(cpr::Url{"http://" + node_addr + "/get_leader"});
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

    bft_raft::request_handler handler {vm["http-port"].as<int>(), server, srv_state_machine};
    handler.run();

    return 0;
}
