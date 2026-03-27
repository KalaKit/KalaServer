//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <cstdint>

namespace KalaServer::TLS
{
    using u8 = uint8_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    static u32 u8_to_u32(const u8* p)
    {
        return (
            ((u32)(p[0]))       | 
            ((u32)(p[1]) << 8)  | 
            ((u32)(p[2]) << 16) | 
            ((u32)(p[3]) << 24));
    };

    static u64 u8_to_u64(const u8* p)
    {
        return (
            ((u64)(p[0]))       | 
            ((u64)(p[1]) << 8)  | 
            ((u64)(p[2]) << 16) | 
            ((u64)(p[3]) << 24) | 
            ((u64)(p[4]) << 32) | 
            ((u64)(p[5]) << 40) | 
            ((u64)(p[6]) << 48) | 
            ((u64)(p[7]) << 56 ));
    }

    static void u64_to_u8(u8* p, u64 v)
    {
        p[0] = (u8)(v      );
        p[1] = (u8)(v >> 8 );
        p[2] = (u8)(v >> 16);
        p[3] = (u8)(v >> 24);
        p[4] = (u8)(v >> 32);
        p[5] = (u8)(v >> 40);
        p[6] = (u8)(v >> 48);
        p[7] = (u8)(v >> 56);
    }
}