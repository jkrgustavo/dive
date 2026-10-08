#include "player.h"
#include "mat_math.h"
#include "camera.h"

void player_init(struct Player *player, double3 pos) {
    player->position = pos;
    player->movespeed = simd_make_double3(5.0, 5.0, 5.0);
    player->mouse_sensitivity = 0.005;
    player->height = 2;
    player->flags.sprint = false;
}

static void update_mouse_lock(const struct Input *input) {
    if (input_key_pressed(input, SAPP_KEYCODE_Q)) {
        if (sapp_mouse_locked()) {
            sapp_lock_mouse(false);
            igGetIO()->ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            sapp_lock_mouse(true);
            igGetIO()->ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
    }
}

static double3 derive_move_intent(struct Player *player, const struct Input *input, f32 pitch, f32 yaw) {
    double3 forward = simd_double(derive_forward(pitch, yaw));
    double3 right = simd_make_double3(cos(yaw), 0.0, sin(yaw));
    double3 up = simd_make_double3(0.0, 1.0, 0.0);
    double3 move_dir = simd_make_double3(0.0, 0.0, 0.0);
    forward.y = 0.0;

    if (input_key_down(input, SAPP_KEYCODE_W)) move_dir += forward;
    if (input_key_down(input, SAPP_KEYCODE_S)) move_dir -= forward;
    if (input_key_down(input, SAPP_KEYCODE_D)) move_dir += right;
    if (input_key_down(input, SAPP_KEYCODE_A)) move_dir -= right;
    if (input_key_down(input, SAPP_KEYCODE_SPACE)) move_dir += up;
    if (input_key_down(input, SAPP_KEYCODE_LEFT_SHIFT)) move_dir -= up;
    if (input_key_pressed(input, SAPP_KEYCODE_F)) player->flags.sprint = !player->flags.sprint;

    double3 speedmod = player->flags.sprint ? player->movespeed * 2.0 : player->movespeed;

    return simd_normalize(move_dir) * speedmod;
}

void player_update(struct Player *player, const struct Camera *cam, const struct Input *input) {
    if (input_key_pressed(input, SAPP_KEYCODE_ESCAPE))
        sapp_request_quit();

    update_mouse_lock(input);

    double3 movement = derive_move_intent(player, input, cam->pitch, cam->yaw);

    if (simd_length_squared(movement) > 0.0)
        player->position += movement * sapp_frame_duration();
    
}
