#include <CLI/CLI.hpp>
#include <crow.h>
#include <cpr/cpr.h>

#include <cryptopp/rsa.h>
#include <cryptopp/sha.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <cryptopp/base64.h>

#include <unordered_map>

using namespace CryptoPP;

constexpr std::string CLUISTER_ENTRY_POINT = "localhost:8080";

void connect_to_cluster(int node_id,
                        const std::string& node_rpc_ep,
                        const std::string& node_http_ep,
                        const std::string& connection_str) {
    auto response = cpr::Get(cpr::Url{"http://" + connection_str + "/get_leader"});
    auto resp_json = crow::json::load(response.text);
    std::string leader_addr = resp_json["http_ep"].s();
    
    crow::json::wvalue req;
    req["id"] = node_id;
    req["rpc_ep"] = node_rpc_ep;
    req["http_ep"] = node_http_ep;

    response = cpr::Post(
        cpr::Url{"http://" + leader_addr + "/register_node"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{req.dump()}
    );
}

int main(int argc, char* argv[]) {
    CLI::App app;
    std::string config_path {};
    int http_port {};

    app.add_option("--config", config_path, "JSON file with seed nodes");
    app.add_option("-p", http_port, "HTTP server port")->required(true);

    CLI11_PARSE(app, argc, argv);

    AutoSeededRandomPool rng;
    RSA::PrivateKey privateKey;
    RSA::PublicKey publicKey;

    privateKey.GenerateRandomWithKeySize(rng, 2048);
    publicKey.AssignFrom(privateKey);

    std::unordered_map<int, std::string> http_endpoints;
    if (!config_path.empty()) {
        std::ifstream file(config_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();
        auto json = crow::json::load(json_str);

        for (const auto& ep : json) {
            http_endpoints[int32_t(ep["id"].i())] = ep["http_ep"].s();
        }
    }

    RSASS<PKCS1v15, SHA256>::Signer signer(privateKey);
    SecByteBlock signature(signer.MaxSignatureLength());
    std::string signatureBase64;
    Base64Encoder encoder(new StringSink(signatureBase64));
    encoder.Put(signature, signature.size());
    encoder.MessageEnd();

    crow::SimpleApp http_server;

    CROW_ROUTE(http_server, "/register_node").methods(crow::HTTPMethod::Post)
    ([http_endpoints, signatureBase64](const crow::request& req) mutable {
        auto req_json = crow::json::load(req.body);
        auto new_node_id = req_json["id"].i();
        auto new_node_http_ep = req_json["http_ep"].s();
        auto new_node_rpc_ep = req_json["rpc_ep"].s();
        http_endpoints[new_node_id] = new_node_http_ep;

        connect_to_cluster(new_node_id, new_node_rpc_ep, new_node_http_ep, CLUISTER_ENTRY_POINT);

        crow::json::wvalue resp;
        resp["status"] = "OK";
        resp["signature"] = signatureBase64;
        return crow::response(resp);
    });

    http_server.port(http_port).multithreaded().run();
    return 0;
}