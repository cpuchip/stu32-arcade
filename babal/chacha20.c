/* rand 0.10's ChaCha20Rng, as row_generator.rs uses it: written from the ChaCha20 block
 * function (RFC 8439 section 2.3, D. J. Bernstein), not from rand's source. The layout was
 * pinned black-box against rand 0.10.2 (oracle/ref `pins`): the 32-byte seed is the key,
 * words 12-13 a 64-bit block counter from 0, words 14-15 a stream id of 0; output words in
 * order; a u64 is the low word then the high; an f64 is the top 53 bits times 2^-53. */
#include <string.h>

#include "babal.h"

#define ROTL(a, b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d)                                     \
    do {                                                   \
        a += b; d ^= a; d = ROTL(d, 16);                   \
        c += d; b ^= c; b = ROTL(b, 12);                   \
        a += b; d ^= a; d = ROTL(d, 8);                    \
        c += d; b ^= c; b = ROTL(b, 7);                    \
    } while (0)

static void refill(bb_chacha20 *rng)
{
    uint32_t s[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
    uint32_t x[16];
    memcpy(s + 4, rng->key, sizeof rng->key);
    s[12] = (uint32_t)rng->counter;
    s[13] = (uint32_t)(rng->counter >> 32);
    s[14] = 0;
    s[15] = 0;
    memcpy(x, s, sizeof s);
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; i++)
        rng->buf[i] = x[i] + s[i];
    rng->counter++;
    rng->index = 0;
}

void bb_chacha20_from_seed(bb_chacha20 *rng, const uint8_t seed[32])
{
    for (int i = 0; i < 8; i++)
        rng->key[i] = (uint32_t)seed[4 * i] | (uint32_t)seed[4 * i + 1] << 8 |
                      (uint32_t)seed[4 * i + 2] << 16 | (uint32_t)seed[4 * i + 3] << 24;
    rng->counter = 0;
    rng->index = 16;
}

static uint32_t next_u32(bb_chacha20 *rng)
{
    if (rng->index >= 16)
        refill(rng);
    return rng->buf[rng->index++];
}

uint64_t bb_chacha20_next_u64(bb_chacha20 *rng)
{
    uint64_t lo = next_u32(rng);
    return lo | (uint64_t)next_u32(rng) << 32;
}

double bb_chacha20_random_f64(bb_chacha20 *rng)
{
    return (double)(bb_chacha20_next_u64(rng) >> 11) * (1.0 / 9007199254740992.0);
}
