#include "input.h"

static void handle_mouse(struct Input *input, const sapp_event *e) {
    input->mouse_pos = simd_make_float2(e->mouse_x, e->mouse_y);
    input->mouse_delta += simd_make_float2(e->mouse_dx, e->mouse_dy); 
}

void input_handle(struct Input *input, const sapp_event *e) {
    switch (e->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (!e->key_repeat) input->keys[e->key_code] |= KEY_PRESSED;
            input->keys[e->key_code] |= KEY_DOWN;
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            input->keys[e->key_code] |= KEY_RELEASED;
            input->keys[e->key_code] &= ~KEY_DOWN;
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            handle_mouse(input, e);
            break;
        case SAPP_EVENTTYPE_SUSPENDED:
        case SAPP_EVENTTYPE_ICONIFIED:
        case SAPP_EVENTTYPE_UNFOCUSED:
            memset(input->keys, 0, sizeof(input->keys));
            input->mouse_delta = simd_make_float2(0.0f, 0.0f);
            break;
        default: break;
    };
}

void input_update(struct Input *input) {
    input->mouse_delta = simd_clamp(input->mouse_delta, -100.0f, 100.0f);
}

void input_end_frame(struct Input *input) {
    input->mouse_delta = simd_make_float2(0.0f, 0.0f);
    for (int i = 0; i < KEY_ARRAY_SIZE; i++) {
        input->keys[i] &= ~(KEY_RELEASED | KEY_PRESSED);
    }
}

void input_init(struct Input *input) {
    input->mouse_delta = simd_make_float2(0.0f, 0.0f);
    input->mouse_pos = simd_make_float2(0.0f, 0.0f);
    memset(input->keys, 0, sizeof(input->keys));
}


