/* Rust's `as` from f64 to an integer saturates (NaN gives 0); C's cast is undefined out of range. */
#ifndef BABAL_INTERNAL_H
#define BABAL_INTERNAL_H

#include <stdint.h>

static inline int64_t bb_f64_as_i64(double x)
{
    if (x != x)
        return 0;
    if (x >= 9223372036854775808.0)
        return INT64_MAX;
    if (x < -9223372036854775808.0)
        return INT64_MIN;
    return (int64_t)x;
}

static inline int32_t bb_f64_as_i32(double x)
{
    if (x != x)
        return 0;
    if (x >= 2147483648.0)
        return INT32_MAX;
    if (x < -2147483648.0)
        return INT32_MIN;
    return (int32_t)x;
}

static inline intptr_t bb_f64_as_isize(double x)
{
#if INTPTR_MAX == INT64_MAX
    return (intptr_t)bb_f64_as_i64(x);
#else
    return (intptr_t)bb_f64_as_i32(x);
#endif
}

/* Rust's std::f64::consts::PI: the same double as POSIX's M_PI, which strict C11 leaves out. */
#define BB_PI 3.14159265358979323846264338327950288

static inline intptr_t bb_imax(intptr_t a, intptr_t b) { return a > b ? a : b; }
static inline intptr_t bb_imin(intptr_t a, intptr_t b) { return a < b ? a : b; }

#endif
