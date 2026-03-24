#include "in_memory_state_mgr.hxx"
#include "bft_state_machine.hpp"
#include "request_handler.hpp"

#include <libnuraft/nuraft.hxx>
#include <crow.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
using namespace nuraft;

int main(int argc, char* argv[]) {
    po::options_description desc {"Allowed options"};
    desc.add_options()
        ("id", po::value<int>()->required(), "Raft server ID")
        ("rpc_port", po::value<int>()->required(), "Raft server RPC port")
        ("http_port", po::value<int>()->required(), "Server HTTP port for REST API");
    
    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cout << desc << std::endl;
        return 0;
    }

    auto srv_state_mgr = cs_new<inmem_state_mgr>(vm["id"].as<int>(), "localhost:" + std::to_string(vm["rpc_port"].as<int>()));

    auto srv_state_machine = cs_new<bft_raft::bft_state_machine>();
    srv_state_machine->add_http_endpoint(vm["id"].as<int>(), "localhost:" + std::to_string(vm["http_port"].as<int>()));

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
        vm["rpc_port"].as<int>(),
        asio_opt,
        params
    );

    while (!server->is_initialized()) {
        std::this_thread::sleep_for( std::chrono::milliseconds(100) );
    }

    bft_raft::request_handler handler {vm["http_port"].as<int>(), server, srv_state_machine};
    handler.run();

    return 0;
}
