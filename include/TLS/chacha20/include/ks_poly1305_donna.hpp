//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <memory>

#include "core_utils.hpp"

namespace KalaServer::TLS
{
#define poly1305_block_size 16

    using std::unique_ptr;

    using u8 = uint8_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    struct LIB_API Poly1305Context
    {
        //clamped key
        u64 r[3]{};
        //accumulator
        u64 h[3]{};
        //second half of key
        u64 pad[2]{};
        //how many bytes are currently in buffer
        size_t leftover{};
        //partial block storage
        u8 buffer[poly1305_block_size]{};
        //is this the last block
        u8 final{};
    };

    class LIB_API Poly1305
    {
    public:
        static unique_ptr<Poly1305Context> CreateContext(const u8 key[32]);
    };
}