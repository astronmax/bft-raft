#include <cpr/cpr.h>
#include <crow.h>

#include <cryptopp/rsa.h>
#include <cryptopp/sha.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <cryptopp/base64.h>

#include <CLI/CLI.hpp>

using namespace CryptoPP;

int main(int argc, char* argv[]) {
    std::string message;
    std::string addr;

    CLI::App app;
    app.add_option("-m", message, "Message to send to cluster")->required(true);
    app.add_option("-e", addr, "Endpoint from cluster to send")->required(true);

    CLI11_PARSE(app, argc, argv);

    AutoSeededRandomPool rng;
    RSA::PrivateKey privateKey;
    RSA::PublicKey publicKey;

    privateKey.GenerateRandomWithKeySize(rng, 2048);
    publicKey.AssignFrom(privateKey);

    RSASS<PKCS1v15, SHA256>::Signer signer(privateKey);
    size_t signatureLength = signer.MaxSignatureLength();
    SecByteBlock signature(signatureLength);

    crow::json::wvalue req;
    req["data"] = message;

    signer.SignMessage(rng, (const byte*)message.data(), message.size(), signature);
    std::string signatureBase64;
    Base64Encoder encoder(new StringSink(signatureBase64));
    encoder.Put(signature, signature.size());
    encoder.MessageEnd();
    req["signature"] = signatureBase64;

    ByteQueue queue;
    publicKey.DEREncodePublicKey(queue);
    std::string binaryKey;
    StringSink sink(binaryKey);
    queue.TransferTo(sink);
    
    std::string base64Key;
    StringSource ss(binaryKey, true,
        new Base64Encoder(new StringSink(base64Key))
    );

    req["pubkey"] = base64Key;

    auto response = cpr::Get(cpr::Url{"http://" + addr + "/get_leader"});
    auto resp_json = crow::json::load(response.text);
    std::string leader_addr = resp_json["http_ep"].s();

    std::cout << "[+] Sending request: " << req.dump() << std::endl;

    response = cpr::Post(
        cpr::Url{"http://" + leader_addr + "/append_data"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{req.dump()}
    );

    std::cout << "[+] Response: " << response.text << std::endl;

    return 0;
}
