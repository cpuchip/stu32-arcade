/* euclid::default::Vector3D<f64>::lerp, pinned black-box against euclid 0.22 (oracle/ref `pins`). */
#include "babal.h"

bb_vec3 bb_vec3_lerp(bb_vec3 a, bb_vec3 b, double t)
{
    double one_t = 1.0 - t;
    bb_vec3 r = {a.x * one_t + b.x * t, a.y * one_t + b.y * t, a.z * one_t + b.z * t};
    return r;
}
