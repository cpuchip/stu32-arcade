/* SPDX-License-Identifier: MIT */
/* from babalcore input_steer.rs, commit fa985cd. Each HashMap<i64, KeyState> is a small
 * array in insertion order; see docs/stage1-rules.md, quirk 3. */
#include "babal.h"

static void key_state_new(bb_key_state *k, int64_t id, double intensity, bb_instant now)
{
    k->id = id;
    k->acc = BB_BASE_KEYBOARD_ACC_VALUE;
    k->intensity = intensity;
    k->has_last_press = true;
    k->last_press = now;
}

static void key_state_bump(bb_key_state *k, double intensity, bb_instant now)
{
    if (k->has_last_press) {
        if (now < k->last_press)
            return;
        uint64_t d = (uint64_t)(now - k->last_press);
        k->acc += k->intensity * bb_duration_as_secs_f64(d);
    } else {
        k->acc = BB_BASE_KEYBOARD_ACC_VALUE;
    }
    k->intensity = intensity;
    k->has_last_press = true;
    k->last_press = now;
}

static void key_state_release(bb_key_state *k, bb_instant now)
{
    if (k->has_last_press) {
        key_state_bump(k, k->intensity, now);
        /* Pay back the BASE_KEYBOARD_ACC_VALUE the key started with. */
        k->acc -= BB_BASE_KEYBOARD_ACC_VALUE;
        if (k->acc < 0.0)
            k->acc = 0.0;
        k->intensity = 0.0;
        k->has_last_press = false;
    }
}

static double key_state_flush(bb_key_state *k, bb_instant now)
{
    if (k->has_last_press)
        key_state_bump(k, k->intensity, now);
    double acc = k->acc;
    k->acc = 0.0;
    return acc;
}

void bb_input_steer_init(bb_input_steer *is)
{
    is->mouse_sensibility = BB_DEFAULT_MOUSE_SENSIBILITY;
    is->keyboard_sensibility = BB_DEFAULT_KEYBOARD_SENSIBILITY;
    is->mouse_acc = 0.0;
    is->right_keys.n = 0;
    is->left_keys.n = 0;
    is->dropped_keys = 0;
}

static double flush_mouse(bb_input_steer *is)
{
    double acc = is->mouse_acc;
    is->mouse_acc = 0.0;
    return acc;
}

void bb_input_steer_mouse_move(bb_input_steer *is, double delta_px)
{
    is->mouse_acc += delta_px;
}

static bb_key_state *find_key(bb_key_map *keys, int64_t id)
{
    for (int i = 0; i < keys->n; i++)
        if (keys->v[i].id == id)
            return &keys->v[i];
    return NULL;
}

static void bump_key(bb_key_map *keys, int64_t id, double intensity, bb_instant now, uint32_t *dropped)
{
    intensity = intensity > BB_INTENSITY_MIN ? (intensity < BB_INTENSITY_MAX ? intensity : BB_INTENSITY_MAX)
                                             : BB_INTENSITY_MIN;
    if (intensity <= 0.0)
        return;
    bb_key_state *k = find_key(keys, id);
    if (k)
        key_state_bump(k, intensity, now);
    else if (keys->n < BB_STEER_KEYS_MAX)
        key_state_new(&keys->v[keys->n++], id, intensity, now);
    else
        (*dropped)++;
}

static void release_key(bb_key_map *keys, int64_t id, bb_instant now)
{
    bb_key_state *k = find_key(keys, id);
    if (k)
        key_state_release(k, now);
}

static double flush_keys(bb_key_map *keys, bb_instant now)
{
    double acc = 0.0;
    for (int i = 0; i < keys->n; i++)
        acc += key_state_flush(&keys->v[i], now);
    return acc;
}

static void handle_key(bb_key_map *keys, int64_t id, double intensity, bool state, bb_instant now, uint32_t *dropped)
{
    if (state)
        bump_key(keys, id, intensity, now, dropped);
    else
        release_key(keys, id, now);
}

void bb_input_steer_right_key(bb_input_steer *is, int64_t id, double intensity, bool state, bb_instant now)
{
    handle_key(&is->right_keys, id, intensity, state, now, &is->dropped_keys);
}

void bb_input_steer_left_key(bb_input_steer *is, int64_t id, double intensity, bool state, bb_instant now)
{
    handle_key(&is->left_keys, id, intensity, state, now, &is->dropped_keys);
}

double bb_input_steer_pop_steer(bb_input_steer *is, bb_instant now)
{
    double mouse_steer = flush_mouse(is);
    double right = flush_keys(&is->right_keys, now);
    double key_steer = right - flush_keys(&is->left_keys, now);
    return mouse_steer * is->mouse_sensibility + key_steer * is->keyboard_sensibility;
}
