/* from babalcore slab_def.rs, slab_kind.rs, slab.rs, commit fa985cd */
#include "babal.h"

bool bb_slab_def_is_special(bb_slab_def def)
{
    switch (def) {
    case BB_DEF_VOID: return false;
    case BB_DEF_FLOOR: return false;
    case BB_DEF_BOOST: return true;
    }
    return false;
}

char bb_slab_def_char(bb_slab_def def)
{
    switch (def) {
    case BB_DEF_VOID: return ' ';
    case BB_DEF_FLOOR: return '#';
    case BB_DEF_BOOST: return '^';
    }
    return '?';
}

int64_t bb_slab_kind_as_item(bb_slab_kind kind)
{
    return (int64_t)kind;
}

bb_slab_kind bb_slab_kind_from_item(int64_t item)
{
    switch (item) {
    case 0: return BB_KIND_FLOOR;
    case 1: return BB_KIND_BOOST;
    case 2: return BB_KIND_OVERDRIVE;
    case 3: return BB_KIND_HIGHLIGHT;
    default: return BB_KIND_VOID;
    }
}

char bb_slab_kind_char(bb_slab_kind kind)
{
    switch (kind) {
    case BB_KIND_VOID: return ' ';
    case BB_KIND_FLOOR: return '#';
    case BB_KIND_BOOST: return '^';
    case BB_KIND_OVERDRIVE: return 'O';
    case BB_KIND_HIGHLIGHT: return '*';
    }
    return '?';
}

bb_slab bb_slab_new(bb_slab_def def)
{
    bb_slab s = {(uint8_t)def};
    return s;
}

bb_slab_kind bb_slab_kind_of(const bb_slab *slab, int64_t now_msec)
{
    (void)now_msec;
    switch ((bb_slab_def)slab->def) {
    case BB_DEF_VOID: return BB_KIND_VOID;
    case BB_DEF_FLOOR: return BB_KIND_FLOOR;
    case BB_DEF_BOOST: return BB_KIND_BOOST;
    }
    return BB_KIND_VOID;
}

int64_t bb_slab_get_item(const bb_slab *slab, int64_t now_msec)
{
    return bb_slab_kind_as_item(bb_slab_kind_of(slab, now_msec));
}

void bb_slab_set(bb_slab *slab, bb_slab_def def)
{
    slab->def = (uint8_t)def;
}

bb_slab_def bb_slab_get(const bb_slab *slab)
{
    return (bb_slab_def)slab->def;
}
