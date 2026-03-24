#pragma once

#include <libnuraft/nuraft.hxx>

#include <unordered_map>

namespace bft_raft {

class bft_state_machine : public nuraft::state_machine {
public:
    bft_state_machine();
    ~bft_state_machine() override = default;

    nuraft::ptr<nuraft::buffer> pre_commit(const ulong log_idx, nuraft::buffer& data) override;
    nuraft::ptr<nuraft::buffer> commit(const ulong log_idx, nuraft::buffer& data) override;
    void commit_config(const ulong log_idx, nuraft::ptr<nuraft::cluster_config>& new_conf) override;
    void rollback(const ulong log_idx, nuraft::buffer& data) override;
    bool apply_snapshot(nuraft::snapshot& s) override;
    nuraft::ptr<nuraft::snapshot> last_snapshot() override;
    ulong last_commit_index() override;
    void create_snapshot(nuraft::snapshot& s, nuraft::async_result<bool>::handler_type& when_done) override;
    
    void add_http_endpoint(int id, std::string ep);
    std::string get_http_endpoint(int id);

private:
    std::atomic<uint64_t> _last_committed_idx;
    nuraft::ptr<nuraft::snapshot> _last_snapshot;
    std::mutex _last_snapshot_lock;
    std::unordered_map<int, std::string> _http_endpoints;
};

};
