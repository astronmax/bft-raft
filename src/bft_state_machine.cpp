#include "bft_state_machine.hpp"
#include "request_handler.hpp"

#include <crow/json.h>
#include <crypto++/filters.h>
#include <crypto++/secblock.h>
#include <cryptopp/rsa.h>
#include <cryptopp/sha.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <cryptopp/base64.h>

#include <iostream>

using namespace bft_raft;
using namespace nuraft;

bft_state_machine::bft_state_machine()
    : _last_committed_idx(0)
{
}

ptr<buffer> bft_state_machine::pre_commit(const ulong log_idx, buffer& data) {
    buffer_serializer bs(data);
    std::string str = bs.get_str();

    std::cout << "pre_commit " << log_idx << ": " << str << std::endl;
    return nullptr;
}

ptr<buffer> bft_state_machine::commit(const ulong log_idx, buffer& data) {
    buffer_serializer bs(data);
    std::string str = bs.get_str();

    auto leader_req = crow::json::load(str);
    if (leader_req["type"] == get_leader_request_type{}[leader_request_type::REGISTER_NODE]) {
        auto node_id = leader_req["node_id"].i();
        auto node_http_ep = leader_req["node_http_ep"].s();
        this->add_http_endpoint(node_id, node_http_ep);
    } else if (leader_req["type"] == get_leader_request_type{}[leader_request_type::DATA]) {
        auto signature_b64 = leader_req["signature"].s();
        auto pubkey_b64 = leader_req["pubkey"].s();
        auto msg = leader_req["data"].s();

        std::cout << "CHECK: " << this->check_message(msg, signature_b64, pubkey_b64) << std::endl;
        std::cout << "DATA: " << msg << std::endl;
    } else {
        std::cout << "commit " << log_idx << ": " << str << std::endl;
    }

    _last_committed_idx = log_idx;
    return nullptr;
}

void bft_state_machine::commit_config(const ulong log_idx, ptr<cluster_config>& new_conf) {
    _last_committed_idx = log_idx;
}

void bft_state_machine::rollback(const ulong log_idx, buffer& data) {
    buffer_serializer bs(data);
    std::string str = bs.get_str();

    std::cout << "rollback " << log_idx << ": "
                << str << std::endl;
}

bool bft_state_machine::apply_snapshot(snapshot& s) {
    std::cout << "apply snapshot " << s.get_last_log_idx()
                << " term " << s.get_last_log_term() << std::endl;

    {   std::lock_guard<std::mutex> l(_last_snapshot_lock);
        ptr<buffer> snp_buf = s.serialize();
        _last_snapshot = snapshot::deserialize(*snp_buf);
    }
    return true;
}

ptr<snapshot> bft_state_machine::last_snapshot() {
    std::lock_guard<std::mutex> l(_last_snapshot_lock);
    return _last_snapshot;
}

ulong bft_state_machine::last_commit_index() {
    return _last_committed_idx;
}

void bft_state_machine::create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done) {
    std::cout << "create snapshot " << s.get_last_log_idx()
                  << " term " << s.get_last_log_term() << std::endl;

    {   std::lock_guard<std::mutex> l(_last_snapshot_lock);
        ptr<buffer> snp_buf = s.serialize();
        _last_snapshot = snapshot::deserialize(*snp_buf);
    }
    ptr<std::exception> except(nullptr);
    bool ret = true;
    when_done(ret, except);
}

void bft_state_machine::add_http_endpoint(int id, std::string ep) {
    _http_endpoints[id] = ep;
}

std::string bft_state_machine::get_http_endpoint(int id) {
    return _http_endpoints.at(id);
}

std::unordered_map<int, std::string> bft_state_machine::get_http_endpoints() {
    return _http_endpoints;
}

bool bft_state_machine::check_message(const std::string& msg,
                       const std::string& signature_b64,
                       const std::string& pubkey_b64) const {
    std::string binaryKey;
    CryptoPP::StringSource ss(pubkey_b64, true,
        new CryptoPP::Base64Decoder(new CryptoPP::StringSink(binaryKey))
    );
    CryptoPP::ByteQueue queue;
    queue.Put((const byte*)binaryKey.data(), binaryKey.size());

    CryptoPP::RSA::PublicKey publicKey;
    publicKey.BERDecodePublicKey(queue, false, 0);

    std::string decodedSignature;
    CryptoPP::StringSource ss2(signature_b64, true,
        new CryptoPP::Base64Decoder(
            new CryptoPP::StringSink(decodedSignature)
        )
    );

    CryptoPP::RSASS<CryptoPP::PKCS1v15, CryptoPP::SHA256>::Verifier verifier(publicKey);
    return verifier.VerifyMessage(
        (const CryptoPP::byte*) msg.data(), msg.size(),
        (const CryptoPP::byte*) decodedSignature.data(), decodedSignature.size());
}
