/* from babalcore row_generator.rs, commit fa985cd */
#include "babal.h"

static void build_rng_seed(uint8_t buf[32], uint64_t seed, size_t row_index)
{
    /* Little-endian u64s: the seed, then the row index, then zeros. */
    uint64_t j = (uint64_t)row_index;
    for (int i = 0; i < 32; i++)
        buf[i] = 0;
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)(seed >> (8 * i));
        buf[8 + i] = (uint8_t)(j >> (8 * i));
    }
}

static void row_from_def_density(bb_row *row, bb_chacha20 *rng, size_t width, const bb_def_densities *v)
{
    bb_row_new(row, BB_DEF_VOID);
    for (size_t i = 0; i < width; i++) {
        double f = bb_chacha20_random_f64(rng);
        double limit = 0.0;
        for (int k = 0; k < v->n; k++) {
            if (f >= limit && f < limit + v->v[k].density) {
                bb_row_set(row, (intptr_t)i, v->v[k].def);
                break;
            }
            limit += v->v[k].density;
        }
    }
}

static void row_from_previous(bb_row *row, bb_chacha20 *rng, const bb_row *previous_row,
                              const bb_def_densities *v, double mutability)
{
    bb_row_new(row, BB_DEF_VOID);
    for (intptr_t i = 0; i < BB_WIDTH_MAX; i++) {
        double f = bb_chacha20_random_f64(rng);
        double limit = 0.0;
        for (int k = 0; k < v->n; k++) {
            if (f >= limit && f < limit + v->v[k].density) {
                bb_row_set(row, i, v->v[k].def);
                break;
            }
            limit += v->v[k].density;
        }
        double change = bb_chacha20_random_f64(rng);
        bb_slab_def previous_def = bb_row_get(previous_row, i);
        if (change >= mutability && !bb_slab_def_is_special(previous_def) &&
            !bb_slab_def_is_special(bb_row_get(row, i)))
            bb_row_set(row, i, previous_def);
    }
}

void bb_generate_first_row(bb_row *out, uint64_t seed, size_t width, bb_skill skill)
{
    uint8_t buf[32];
    bb_chacha20 rng;
    build_rng_seed(buf, seed, 0);
    bb_chacha20_from_seed(&rng, buf);
    bb_def_densities def_density = bb_skill_def_density0(skill);
    row_from_def_density(out, &rng, width, &def_density);
}

void bb_generate_next_row(bb_row *out, uint64_t seed, size_t j, const bb_row *previous_row, bb_skill skill)
{
    uint8_t buf[32];
    bb_chacha20 rng;
    build_rng_seed(buf, seed, j);
    bb_chacha20_from_seed(&rng, buf);
    bb_def_densities def_density = bb_skill_def_density(skill);
    row_from_previous(out, &rng, previous_row, &def_density, bb_skill_mutability(skill));
}
