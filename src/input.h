#ifndef INPUT_H
#define INPUT_H

#include "util.h"

#define KEY_DOWN 0x01
#define KEY_PRESSED 0x02
#define KEY_RELEASED 0x04

#define KEY_ARRAY_SIZE (SAPP_KEYCODE_MENU + 1)

struct Input {
    /* packed u8
     *  - bit 0: whether key is being held down
     *  - bit 1: whether key was pressed
     *  - bit 2: whether key was released
     *
     *  348 items since it's the greatest enum value in
     *  sokol's keycodes.
     */
    u8 keys[KEY_ARRAY_SIZE];

    float2 mouse_pos, mouse_delta;
};


void input_handle(struct Input *input, const sapp_event *e);
void input_update(struct Input *input);
void input_init(struct Input *input);
void input_end_frame(struct Input *input);

static inline bool input_key_down(const struct Input *input, sapp_keycode key) {
    return (key < KEY_ARRAY_SIZE) && (input->keys[key] & KEY_DOWN);
}

static inline bool input_key_pressed(const struct Input *input, sapp_keycode key) {
    return (key < KEY_ARRAY_SIZE) && (input->keys[key] & KEY_PRESSED);
}

static inline bool input_key_released(const struct Input *input, sapp_keycode key) {
    return (key < KEY_ARRAY_SIZE) && (input->keys[key] & KEY_RELEASED);
}

#endif /* INPUT_H */
