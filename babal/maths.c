/* SPDX-License-Identifier: MIT */
/* from babalcore maths.rs, commit fa985cd */
#include <math.h>

#include "babal.h"
#include "internal.h"

double bb_fade_in(double x)
{
    if (x < 0.0)
        return 0.0;
    else if (x >= 0.0 && x <= 1.0)
        return x;
    else if (x > 1.0)
        return 1.0;
    return NAN;
}

double bb_fade_out(double x)
{
    if (x < 0.0)
        return 1.0;
    else if (x >= 0.0 && x <= 1.0)
        return 1.0 - x;
    else if (x > 1.0)
        return 0.0;
    return NAN;
}

double bb_square_wave(double x)
{
    double f = x - floor(x);
    if (f < 0.5)
        return 1.0;
    else if (f > 0.5)
        return -1.0;
    else if (f == 0.0 || f == 0.5)
        return 0.5;
    return NAN;
}

double bb_triangle_wave(double x)
{
    double f = x - floor(x);
    if (f < 0.25)
        return f * 4.0;
    else if (f >= 0.25 && f < 0.75)
        return 1.0 - (f - 0.25) * 4.0;
    else if (f >= 0.75 && f <= 1.0)
        return -1.0 + (f - 0.75) * 4.0;
    return NAN;
}

double bb_sawtooth_wave(double x)
{
    double f = x - floor(x);
    if (f < 0.5)
        return f * 2.0;
    else if (f > 0.5 && f <= 1.0)
        return -1.0 + (f - 0.5) * 2.0;
    else if (f == 0.5)
        return 0.0;
    return NAN;
}

double bb_sine_wave(double x)
{
    return sin(2.0 * BB_PI * x);
}

double bb_pulse(double x, double alpha)
{
    double f = x - floor(x);
    if (f < alpha)
        return 1.0;
    else if (f > alpha)
        return 0.0;
    else if (f == 0.0 || f == alpha) {
        if (alpha <= 0.0)
            return 0.0;
        else if (alpha >= 1.0)
            return 1.0;
        return 0.5;
    }
    return NAN;
}

double bb_boxcar(double x)
{
    return bb_heaviside_step(x) - bb_heaviside_step(x - 1.0);
}

double bb_heaviside_step(double x)
{
    if (x < 0.0)
        return 0.0;
    else if (x > 0.0)
        return 1.0;
    else if (x == 0.0)
        return 0.5;
    return NAN;
}
