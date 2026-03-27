//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>
#include <cstring>

#include "ks_chacha20.hpp"
#include "ks_tls.hpp"

using KalaServer::TLS::ChaCha20Context;
using KalaServer::TLS::u8_to_u32;

using std::make_unique;
using std::memcpy;

using u8 = uint8_t;
using u32 = uint32_t;

static const u8 magic[] = "expand 32-byte k";

//Sets initial state of all state values
static void SetState(
    const u8 key[32],
    const u8 nonce[12],
    u32 state[16])
{
    state[0] = u8_to_u32(magic + 0 * 4);
    state[1] = u8_to_u32(magic + 1 * 4);
    state[2] = u8_to_u32(magic + 2 * 4);
    state[3] = u8_to_u32(magic + 3 * 4);
    state[4] = u8_to_u32(key + 0 * 4);
    state[5] = u8_to_u32(key + 1 * 4);
    state[6] = u8_to_u32(key + 2 * 4);
    state[7] = u8_to_u32(key + 3 * 4);
    state[8] = u8_to_u32(key + 4 * 4);
    state[9] = u8_to_u32(key + 5 * 4);
    state[10] = u8_to_u32(key + 6 * 4);
    state[11] = u8_to_u32(key + 7 * 4);
    state[12] = 0;
    state[13] = u8_to_u32(nonce + 0 * 4);
    state[14] = u8_to_u32(nonce + 1 * 4);
    state[15] = u8_to_u32(nonce + 2 * 4);
}

//Rotates a 32-bit number left by n bits
static u32 RotateLeft(u32 x, int n)
{
    return (x << n) | (x >> (32 - n));
};

//Scrambles four values together using addition, rotation and XOR
static void QuarterRound(u32* x, u8 a, u8 b, u8 c, u8 d)
{
    x[a] += x[b]; x[d] = RotateLeft(x[d] ^ x[a], 16);
    x[c] += x[d]; x[b] = RotateLeft(x[b] ^ x[c], 12);
    x[a] += x[b]; x[d] = RotateLeft(x[d] ^ x[a], 8);
    x[c] += x[d]; x[b] = RotateLeft(x[b] ^ x[c], 7);
}

namespace KalaServer::TLS
{
    void ChaCha20::CreatePolyKey(
        const u8 key[32],
        const u8 nonce[12],
        u8 polyKey[32])
    {
        u32 state[16]{};
        u32 working[16]{};

        SetState(key, nonce, state);

        //makes a copy of the original state
        //because the proceeding algorithms will heavily modify the state
        for (int i = 0; i < 16; i++)
        {
            working[i] = state[i];
        }

        for (int i = 0; i < 10; i++)
        {
            QuarterRound(working, 0, 4, 8, 12);
            QuarterRound(working, 1, 5, 9, 13);
            QuarterRound(working, 2, 6, 10, 14);
            QuarterRound(working, 3, 7, 11, 15);

            QuarterRound(working, 0, 5, 10, 15);
            QuarterRound(working, 1, 6, 11, 12);
            QuarterRound(working, 2, 7, 8, 13);
            QuarterRound(working, 3, 4, 9, 14);
        }

        //adds the original back to the mixed state
        for (int i = 0; i < 16; i++)
        {
            working[i] += state[i];
        }

        memcpy(polyKey, working, 32);
    }

    unique_ptr<ChaCha20Context> ChaCha20::CreateContext(
        const u8 key[32],
        const u8 nonce[12],
        u64 counter)
    {
        unique_ptr<ChaCha20Context> ctx = make_unique<ChaCha20Context>();

        auto init = [
            &ctx, 
            &key,
            &nonce]() -> void
            {
                memcpy(
                    ctx->key, 
                    key, 
                    sizeof(ctx->key));
                memcpy(
                    ctx->nonce, 
                    nonce, 
                    sizeof(ctx->nonce));

                SetState(key, nonce, ctx->state);
            };

        auto set_counter = [&ctx]() -> void
            {
                ctx->state[12] = (u32)ctx->counter;
                ctx->state[13] = u8_to_u32(ctx->nonce + 0 * 4) + (u32)(ctx->counter >> 32);
            };

        init();

        ctx->counter = counter;
        set_counter();
        ctx->pos = 64;

        return ctx;
    }

    void ChaCha20::Context_XOR(
        ChaCha20Context* ctx, 
        u8* bytes,
        size_t size_bytes)
    {
        auto block_next = [&ctx]() -> void
            {
                //makes a copy of the original state
                //because the proceeding algorithms will heavily modify the state
                for (int i = 0; i < 16; i++)
                {
                    ctx->keyStream[i] = ctx->state[i];
                }

                for (int i = 0; i < 10; i++)
                {
                    QuarterRound(ctx->keyStream, 0, 4, 8, 12);
                    QuarterRound(ctx->keyStream, 1, 5, 9, 13);
                    QuarterRound(ctx->keyStream, 2, 6, 10, 14);
                    QuarterRound(ctx->keyStream, 3, 7, 11, 15);

                    QuarterRound(ctx->keyStream, 0, 5, 10, 15);
                    QuarterRound(ctx->keyStream, 1, 6, 11, 12);
                    QuarterRound(ctx->keyStream, 2, 7, 8, 13);
                    QuarterRound(ctx->keyStream, 3, 4, 9, 14);
                }

                //adds the original back to the mixed state
                for (int i = 0; i < 16; i++)
                {
                    ctx->keyStream[i] += ctx->state[i];
                }

                //use two 32-bit counters to ensure
                //every 64-byte block uses a different keystyream

                u32* counter = ctx->state + 12;
                counter[0]++;
                if (0 == counter[0]) counter[1]++;
            };

        u8* keyStream8 = (u8*)ctx->keyStream;
        for (size_t i = 0; i < size_bytes; i++)
        {
            if (ctx->pos >= 64)
            {
                block_next();
                ctx->pos = 0;
            }
            bytes[i] ^= keyStream8[ctx->pos];
            ctx->pos++; 
        }
    }
}