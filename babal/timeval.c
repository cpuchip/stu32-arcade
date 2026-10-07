/* std::time::Duration as player input uses it (input_jump.rs, input_steer.rs): nanoseconds.
 * Pinned black-box against Rust 1.98 (oracle/ref `pins`): from_secs_f64 rounds the exact
 * binary value of secs * 10^9 to the nearest nanosecond, ties to even (k/1024 s, an exact
 * x.5 ns, gives the even neighbour); as_secs_f64 is whole seconds plus nanos / 1e9. */
#include <string.h>

#include "babal.h"

typedef struct { uint64_t hi, lo; } u128;

/* m * n for m < 2^64, n < 2^32, without a 128-bit type (the S3 has none). */
static u128 mul_64_32(uint64_t m, uint32_t n)
{
    uint64_t low = (m & 0xffffffffu) * n;
    uint64_t high = (m >> 32) * n;
    u128 r;
    r.lo = low + (high << 32);
    r.hi = (high >> 32) + (r.lo < low);
    return r;
}

static u128 shr(u128 x, int k)
{
    u128 r;
    if (k == 0)
        return x;
    if (k >= 128) {
        r.hi = r.lo = 0;
    } else if (k >= 64) {
        r.hi = 0;
        r.lo = x.hi >> (k - 64);
    } else {
        r.hi = x.hi >> k;
        r.lo = (x.lo >> k) | (x.hi << (64 - k));
    }
    return r;
}

/* x mod 2^k compared with 2^(k-1): -1 below, 0 equal, 1 above (1 <= k < 128). */
static int cmp_half(u128 x, int k)
{
    u128 rem, half;
    if (k < 64) {
        rem.hi = 0;
        rem.lo = x.lo & ((1ULL << k) - 1);
    } else if (k == 64) {
        rem.hi = 0;
        rem.lo = x.lo;
    } else {
        rem.hi = x.hi & ((1ULL << (k - 64)) - 1);
        rem.lo = x.lo;
    }
    half.hi = k - 1 >= 64 ? 1ULL << (k - 1 - 64) : 0;
    half.lo = k - 1 < 64 ? 1ULL << (k - 1) : 0;
    if (rem.hi != half.hi)
        return rem.hi > half.hi ? 1 : -1;
    if (rem.lo != half.lo)
        return rem.lo > half.lo ? 1 : -1;
    return 0;
}

uint64_t bb_duration_from_secs_f64(double secs)
{
    /* Callers pass secs > 0 (input_jump.rs checks); Rust panics on negative or overflow. */
    uint64_t bits;
    memcpy(&bits, &secs, sizeof bits);
    int e = (int)((bits >> 52) & 0x7ff);
    uint64_t m = bits & ((1ULL << 52) - 1);
    if (e == 0x7ff || (bits >> 63))
        return 0;
    if (e == 0)
        e = 1; /* subnormal */
    else
        m |= 1ULL << 52;
    int k = 1075 - e; /* secs = m * 2^-k */
    u128 p = mul_64_32(m, 1000000000u);
    if (k <= 0) {
        if (k < -10 || p.hi)
            return UINT64_MAX;
        return p.lo << -k;
    }
    if (k >= 128)
        return 0;
    u128 q = shr(p, k);
    int c = cmp_half(p, k);
    if (c > 0 || (c == 0 && (q.lo & 1)))
        q.lo += 1;
    return q.lo;
}

double bb_duration_as_secs_f64(uint64_t nanos)
{
    return (double)(nanos / 1000000000u) + (double)(nanos % 1000000000u) / 1e9;
}
