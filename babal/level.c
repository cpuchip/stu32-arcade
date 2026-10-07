/* from babalcore level.rs and level_query.rs, commit fa985cd. The VecDeque is logical: its
 * length and row0 are babalcore's, and the newest `window` rows are held in the caller's
 * storage (see babal.h). Quirks 1 and 2 of docs/stage1-rules.md are kept as they are. */
#include "babal.h"
#include "internal.h"

void bb_level_init(bb_level *lvl, uint64_t seed, bb_skill skill, bb_packed_row *storage, size_t window)
{
    size_t width = bb_skill_width(skill);
    lvl->seed = seed;
    lvl->rows = storage;
    lvl->window = window;
    lvl->len = 0;
    lvl->row0 = 0;
    lvl->w = width > BB_WIDTH_MAX ? BB_WIDTH_MAX : width;
    lvl->skl = skill;
    lvl->now_msec = 0;
    lvl->out_of_window = 0;
    lvl->max_read_depth = 0;
}

static bb_packed_row pack(const bb_row *row)
{
    bb_packed_row v = 0;
    for (int i = 0; i < BB_WIDTH_MAX; i++)
        v |= (bb_packed_row)(row->slabs[i].def & 3) << (2 * i);
    return v;
}

/* The packed row index rows from the front (0, all void, when it is older than the window). */
static bb_packed_row packed_at(const bb_level *lvl, size_t index)
{
    bb_level *mut = (bb_level *)lvl; /* the counters are diagnostics, not level state */
    size_t abs = (size_t)lvl->row0 + index;
    size_t depth = (size_t)bb_level_last(lvl) - 1 - abs;
    if (depth > mut->max_read_depth)
        mut->max_read_depth = depth;
    if (depth >= lvl->window) {
        mut->out_of_window++;
        return 0;
    }
    return lvl->rows[abs % lvl->window];
}

const bb_row *bb_level_row_at(const bb_level *lvl, size_t index)
{
    bb_level *mut = (bb_level *)lvl; /* the scratch row is a cache, not level state */
    bb_packed_row v = packed_at(lvl, index);
    for (int i = 0; i < BB_WIDTH_MAX; i++)
        mut->scratch.slabs[i].def = (uint8_t)((v >> (2 * i)) & 3);
    return &lvl->scratch;
}

void bb_level_push(bb_level *lvl, const bb_row *row)
{
    if (lvl->len >= BB_ABSOLUTE_LENGTH_MAX)
        return;
    /* push_back, then pop_front while longer than the sliding length. */
    if (lvl->len == BB_SLIDING_LENGTH_MAX) {
        lvl->row0 += 1;
        lvl->len -= 1;
    }
    lvl->rows[((size_t)lvl->row0 + lvl->len) % lvl->window] = pack(row);
    lvl->len += 1;
}

void bb_level_generate(bb_level *lvl, size_t n)
{
    if (lvl->len >= BB_ABSOLUTE_LENGTH_MAX)
        return;
    for (size_t k = 0; k < n; k++) {
        size_t j = lvl->len; /* quirk 1: the deque's length, not the absolute row */
        bb_row row;
        if (j == 0)
            bb_generate_first_row(&row, lvl->seed, lvl->w, lvl->skl);
        else
            bb_generate_next_row(&row, lvl->seed, j, bb_level_row_at(lvl, j - 1), lvl->skl);
        bb_level_push(lvl, &row);
    }
}

bb_slab_kind bb_level_slab_kind(const bb_level *lvl, intptr_t col, intptr_t row, bool boost)
{
    intptr_t real_row = row + lvl->row0;
    if (col < 0 || col >= BB_WIDTH_MAX)
        return BB_KIND_VOID;
    if (real_row < 0 || real_row >= (intptr_t)bb_level_len(lvl))
        return BB_KIND_VOID;
    bb_slab s = bb_slab_new((bb_slab_def)((packed_at(lvl, (size_t)real_row) >> (2 * col)) & 3));
    bb_slab_kind v = bb_slab_kind_of(&s, lvl->now_msec); /* what Row::slab_kind gives for col */
    if (v == BB_KIND_BOOST)
        return boost ? BB_KIND_OVERDRIVE : BB_KIND_BOOST;
    return v;
}

void bb_level_set_now_msec(bb_level *lvl, int64_t now_msec)
{
    lvl->now_msec = now_msec;
}

void bb_level_set_now_sec(bb_level *lvl, double now_sec)
{
    lvl->now_msec = bb_f64_as_i64(now_sec * 1000.0);
}

size_t bb_level_width(const bb_level *lvl)
{
    return lvl->w;
}

size_t bb_level_len(const bb_level *lvl)
{
    return lvl->len;
}

intptr_t bb_level_first(const bb_level *lvl)
{
    return lvl->row0;
}

intptr_t bb_level_last(const bb_level *lvl)
{
    return lvl->row0 + (intptr_t)lvl->len;
}

int64_t bb_level_item(const bb_level *lvl, intptr_t col, intptr_t row, bool boost)
{
    /* quirk 2: slab_kind adds row0 again */
    return bb_slab_kind_as_item(bb_level_slab_kind(lvl, col, lvl->row0 + row, boost));
}

bool bb_level_find_start_spot(const bb_level *lvl, intptr_t col, intptr_t row, intptr_t *out_col, intptr_t *out_row)
{
    intptr_t width = (intptr_t)bb_level_width(lvl);
    intptr_t r = bb_imax(bb_level_first(lvl), bb_imin(bb_level_last(lvl) - 1, row));
    intptr_t c = bb_imax(0, bb_imin(width - 1, col));

    if (bb_level_slab_kind(lvl, c, r, false) == BB_KIND_FLOOR) {
        *out_col = c;
        *out_row = r;
        return true;
    }

    intptr_t r2 = r;
    intptr_t d = 1;
    while (r2 >= bb_level_first(lvl)) {
        intptr_t limit = r2 == bb_level_first(lvl) ? width : bb_imin(d, width);
        for (intptr_t c2 = 0; c2 < limit; c2++) {
            intptr_t c3 = c < width / 2 ? (c + c2) % width : (width + c - c2) % width;
            if (bb_level_slab_kind(lvl, c3, r2, false) == BB_KIND_FLOOR) {
                *out_col = c3;
                *out_row = r2;
                return true;
            }
        }
        r2 -= 1;
        d += 1;
    }
    return false;
}

bb_skill bb_level_skill(const bb_level *lvl)
{
    return lvl->skl;
}
