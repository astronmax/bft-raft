#include <cpr/cpr.h>
#include <crow.h>

#include <cryptopp/rsa.h>
#include <cryptopp/sha.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <cryptopp/base64.h>

using namespace CryptoPP;

void send_data(AutoSeededRandomPool& rng, const RSA::PrivateKey& privateKey, std::string addr, std::string message) {
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

    cpr::Post(
        cpr::Url{"http://" + addr + "/append_data"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{req.dump()}
    );
}

int main() {
    AutoSeededRandomPool rng;
    RSA::PrivateKey privateKey;
    RSA::PublicKey publicKey;

    privateKey.GenerateRandomWithKeySize(rng, 2048);
    publicKey.AssignFrom(privateKey);

    send_data(rng, privateKey, "localhost:8080", "Hello, Raft !!!");

    return 0;
}
