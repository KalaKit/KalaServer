//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <memory>

#include "core_utils.hpp"

namespace KalaServer::TLS
{
    using std::unique_ptr;

    using u8 = uint8_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    struct LIB_API ChaCha20Context
    {
        //secret used between receiver and sender for the lifetime of the socket
        u32 keyStream[16]{};
        size_t pos{};

        u8 key[32]{};
        u8 nonce[12]{};
        u64 counter{};

        u32 state[16]{};
    };

    class LIB_API ChaCha20
    {
    public:
        //Create the key poly1305 will need for creating its tag
        static void CreatePolyKey(
            const u8 key[32],
            const u8 nonce[12],
            u8 polyKey[32]);

        //Create a new ChaCha20 context.
        //Pass the key and nonce (base IV) from HKDF
        static unique_ptr<ChaCha20Context> CreateContext(
            const u8 key[32],
            const u8 nonce[12],
            u64 counter = 0);

        //Use an existing context and flip the bits of the data using the generated keystream
        static void Context_XOR(
            ChaCha20Context* ctx, 
            u8* bytes,
            size_t size_bytes);
    };
}