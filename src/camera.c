#include "camera.h"
#include "mat_math.h"
#include <simd/math.h>

#define MOVE_SPEED simd_make_double3(5.0, 5.0, 5.0)
#define PITCH_LIMIT radians(89.0f)

void camera_init(struct Camera *cam, f32 fovy, f32 near, f32 far) {
    cam->pos    = simd_make_double3(2.0, 2.0, -10.0);
    cam->pitch  = 0.0f;
    cam->yaw    = PI;
    cam->near   = near;
    cam->far    = far;
    cam->fovy   = fovy;
    cam->aspect = sapp_widthf() / sapp_heightf();

    cam->view = mat_view_dir(derive_forward(cam->pitch, cam->yaw), simd_make_float3(0.0f, 1.0f, 0.0f));
    cam->proj = mat_proj(cam->fovy, cam->aspect, cam->near);
}

// TODO: delta time
void handle_keys(struct Camera *cam, const struct Input *input) {
    double3 forward = simd_double(derive_forward(cam->pitch, cam->yaw));
    double3 right = simd_make_double3(cos(cam->yaw), 0.0, sin(cam->yaw));
    double3 move_dir = simd_make_double3(0.0, 0.0, 0.0);

    if (input_key_down(input, SAPP_KEYCODE_W)) move_dir += forward;
    if (input_key_down(input, SAPP_KEYCODE_S)) move_dir -= forward;
    if (input_key_down(input, SAPP_KEYCODE_D)) move_dir += right;
    if (input_key_down(input, SAPP_KEYCODE_A)) move_dir -= right;

    if (simd_length_squared(move_dir) > 0.0)
        cam->pos += simd_normalize(move_dir) * (MOVE_SPEED * sapp_frame_duration());
}

void update_mouse_movement(struct Camera *cam, const struct Input *input) {
    cam->yaw   += input->mouse_delta.x * MOUSE_SENSITIVITY;
    cam->pitch -= input->mouse_delta.y * MOUSE_SENSITIVITY;
    cam->pitch = simd_clamp(cam->pitch, -PITCH_LIMIT, PITCH_LIMIT);

    if (cam->yaw > PI) cam->yaw -= 2.0f * PI;
    if (cam->yaw < -PI) cam->yaw += 2.0f * PI;
}

void camera_update(struct Camera *cam, const struct Input *input) {
    update_mouse_movement(cam, input);
    handle_keys(cam, input);

    cam->aspect = sapp_widthf() / sapp_heightf();
    cam->proj = mat_proj(cam->fovy, cam->aspect, cam->near);

    float3 forward = derive_forward(cam->pitch, cam->yaw);
    cam->view = mat_view_dir(forward, simd_make_float3(0.0f, 1.0f, 0.0f));
}
