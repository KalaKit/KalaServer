#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cstring>
#include <string>

#include "ks_chacha20.hpp"

using KalaServer::TLS::ChaCha20;
using KalaServer::TLS::ChaCha20Context;

using std::cout;
using std::ostringstream;
using std::hex;
using std::setw;
using std::setfill;
using std::memcpy;
using std::memcmp;
using std::unique_ptr;
using std::string;

using u8 = uint8_t;

static void StartConnection(
    const u8 key[32],
    const u8 nonce[12],
    unique_ptr<ChaCha20Context>& ctx_sender,
    unique_ptr<ChaCha20Context>& ctx_receiver)
{
    u8 polyKey[32]{};

    ChaCha20::CreatePolyKey(key, nonce, polyKey);

    ctx_sender = ChaCha20::CreateContext(key, nonce, 1);
    ctx_receiver = ChaCha20::CreateContext(key, nonce, 1);
}

static void Send(
    ChaCha20Context* ctx,
    u8 dataOut[64],
    u8 originalOut[64])
{
    //original data
    u8 data[64] = "linux is cool";

    //send out the original data before encryption
    memcpy(originalOut, data, 64);

    ChaCha20::Context_XOR(ctx, data, 64);

    //send out the encrypted data
    memcpy(dataOut, data, 64);

    ostringstream originalStream{};
    originalStream << (char*)originalOut;

    cout << "sender created:\n" << originalStream.str();

    ostringstream encryptedStream{};
    for (int i = 0; i < 64; i++)
    {
        encryptedStream << hex 
            << setw(2) 
            << setfill('0')
            << (int)dataOut[i]
            << " ";

        if ((i + 1) % 16 == 0) encryptedStream << "\n";
    }

    cout << "\n\nsender encrypted:\n" << encryptedStream.str();
}

static void Receive(
    ChaCha20Context* ctx,
    u8 data[64],
    u8 decryptedOut[64])
{
    ostringstream encryptedStream{};
    for (int i = 0; i < 64; i++)
    {
        encryptedStream << hex 
            << setw(2) 
            << setfill('0')
            << (int)data[i]
            << " ";

        if ((i + 1) % 16 == 0) encryptedStream << "\n";
    }

    cout << "\n\nreceiver received:\n" << encryptedStream.str();

    u8 decrypted[64]{};
    memcpy(decrypted, data, 64);

    ChaCha20::Context_XOR(ctx, decrypted, 64);

    //send out the received decrypted data
    memcpy(decryptedOut, decrypted, 64);

    ostringstream decryptedStream{};
    decryptedStream << (char*)decryptedOut;

    cout << "\n\nreceived decrypted:\n" << decryptedStream.str();
}

int main()
{
    u8 key[32]{};
    u8 nonce[12]{};

    for (int i = 0; i < 32; i++) key[i] = (u8)i;
    for (int i = 0; i < 12; i++) nonce[i] = (u8)i + 100;

    unique_ptr<ChaCha20Context> ctx_sender{};
    unique_ptr<ChaCha20Context> ctx_receiver{};
    StartConnection(
        key,
        nonce,
        ctx_sender,
        ctx_receiver);

    u8 dataOut[64]{};
    u8 originalOut[64]{};
    Send(
        ctx_sender.get(),
        dataOut,
        originalOut);

    u8 decryptedOut[64]{};
    Receive(
        ctx_receiver.get(),
        dataOut,
        decryptedOut);

    bool ok = (memcmp(originalOut, decryptedOut, 64) == 0);
    string success = ok ? "TRUE" : "FALSE";

    cout << "\n\nsender original matches receiver decrypted: " << success;

    return 0;
}