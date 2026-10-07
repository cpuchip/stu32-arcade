/* SPDX-License-Identifier: MIT */
/* from babalcore skill.rs, commit fa985cd. def_density's HashMap becomes an array sorted by
 * def, which is the order row_generator.rs sorts it into before use. */
#include <string.h>

#include "babal.h"

static const char *const NAMES[] = {"noob", "easy", "medium", "hard", "expert"};

bb_skill bb_skill_from_str(const char *name)
{
    for (int i = 0; i < 5; i++)
        if (strcmp(name, NAMES[i]) == 0)
            return (bb_skill)i;
    return BB_SKILL_DEFAULT;
}

const char *bb_skill_as_str(bb_skill skill)
{
    return NAMES[skill];
}

size_t bb_skill_width(bb_skill skill)
{
    switch (skill) {
    case BB_NOOB: return 8;
    case BB_EASY: return 9;
    case BB_MEDIUM: return 10;
    case BB_HARD: return 11;
    case BB_EXPERT: return 12;
    }
    return 10;
}

static bb_def_densities two(double floor, double boost)
{
    bb_def_densities d = {{{BB_DEF_FLOOR, floor}, {BB_DEF_BOOST, boost}}, 2};
    return d;
}

bb_def_densities bb_skill_def_density(bb_skill skill)
{
    switch (skill) {
    case BB_NOOB: return two(0.93, 0.02);
    case BB_EASY: return two(0.83, 0.02);
    case BB_MEDIUM: return two(0.7, 0.05);
    case BB_HARD: return two(0.6, 0.1);
    case BB_EXPERT: return two(0.5, 0.1);
    }
    return two(0.7, 0.05);
}

bb_def_densities bb_skill_def_density0(bb_skill skill)
{
    /* The Rust sums in HashMap order; two terms commute, so this is exact (quirk 4). */
    bb_def_densities density = bb_skill_def_density(skill);
    double total = 0.0;
    for (int i = 0; i < density.n; i++)
        total += density.v[i].density;
    bb_def_densities ret = {{{BB_DEF_FLOOR, total}}, 1};
    return ret;
}

double bb_skill_mutability(bb_skill skill)
{
    switch (skill) {
    case BB_NOOB: return 0.05;
    case BB_EASY: return 0.1;
    case BB_MEDIUM: return 0.2;
    case BB_HARD: return 0.3;
    case BB_EXPERT: return 0.5;
    }
    return 0.2;
}

double bb_skill_time_to_complete(bb_skill skill)
{
    switch (skill) {
    case BB_NOOB: return 30.0;
    case BB_EASY: return 90.0;
    case BB_MEDIUM: return 120.0;
    case BB_HARD: return 180.0;
    case BB_EXPERT: return 300.0;
    }
    return 120.0;
}

double bb_skill_begin_speed_factor(bb_skill skill)
{
    switch (skill) {
    case BB_NOOB: return 0.7;
    case BB_EASY: return 0.9;
    case BB_MEDIUM: return 1.0;
    case BB_HARD: return 1.2;
    case BB_EXPERT: return 1.3;
    }
    return 1.0;
}

double bb_skill_end_speed_factor(bb_skill skill)
{
    switch (skill) {
    case BB_NOOB: return 0.7;
    case BB_EASY: return 1.0;
    case BB_MEDIUM: return 1.2;
    case BB_HARD: return 1.4;
    case BB_EXPERT: return 1.7;
    }
    return 1.2;
}
