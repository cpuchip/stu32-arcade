/* SPDX-License-Identifier: MIT */
/* from babalcore row.rs, commit fa985cd */
#include "babal.h"

void bb_row_new(bb_row *row, bb_slab_def def)
{
    for (int i = 0; i < BB_WIDTH_MAX; i++)
        row->slabs[i] = bb_slab_new(def);
}

bb_slab_kind bb_row_slab_kind(const bb_row *row, intptr_t col, int64_t now_msec)
{
    if (col < 0 || col >= BB_WIDTH_MAX)
        return BB_KIND_VOID;
    return bb_slab_kind_of(&row->slabs[col], now_msec);
}

void bb_row_set(bb_row *row, intptr_t col, bb_slab_def def)
{
    if (col < 0 || col >= BB_WIDTH_MAX)
        return;
    bb_slab_set(&row->slabs[col], def);
}

bb_slab_def bb_row_get(const bb_row *row, intptr_t col)
{
    if (col < 0 || col >= BB_WIDTH_MAX)
        return BB_DEF_VOID;
    return bb_slab_get(&row->slabs[col]);
}

void bb_row_format(const bb_row *row, char out[BB_WIDTH_MAX + 1])
{
    /* Display for Row: each Slab displays its slab_kind at time 0. */
    for (int i = 0; i < BB_WIDTH_MAX; i++)
        out[i] = bb_slab_kind_char(bb_slab_kind_of(&row->slabs[i], 0));
    out[BB_WIDTH_MAX] = '\0';
}
