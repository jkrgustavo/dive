#include "camera.h"
#include "mat_math.h"

#define PITCH_LIMIT radians(89.0f)

void camera_init(struct Camera *cam, const struct Player *player, f32 fovy, f32 near, f32 far) {
    cam->pos    = player->position;
    cam->pitch  = 0.0f;
    cam->yaw    = PI;
    cam->near   = near;
    cam->far    = far;
    cam->fovy   = fovy;
    cam->aspect = sapp_widthf() / sapp_heightf();

    cam->view = mat_view_dir(derive_forward(cam->pitch, cam->yaw), simd_make_float3(0.0f, 1.0f, 0.0f));
    cam->proj = mat_proj(cam->fovy, cam->aspect, cam->near);
}

static void update_mouse_movement(struct Camera *cam, const struct Input *input, f32 sensitivity) {
    cam->yaw   += input->mouse_delta.x * sensitivity;
    cam->pitch -= input->mouse_delta.y * sensitivity;
    cam->pitch = simd_clamp(cam->pitch, -PITCH_LIMIT, PITCH_LIMIT);

    if (cam->yaw > PI) cam->yaw -= 2.0f * PI;
    if (cam->yaw < -PI) cam->yaw += 2.0f * PI;
}

void camera_update(struct Camera *cam, const struct Player *player, const struct Input *input) {
    update_mouse_movement(cam, input, player->mouse_sensitivity);

    cam->pos = player->position;

    cam->aspect = sapp_widthf() / sapp_heightf();
    cam->proj = mat_proj(cam->fovy, cam->aspect, cam->near);

    float3 forward = derive_forward(cam->pitch, cam->yaw);
    cam->view = mat_view_dir(forward, simd_make_float3(0.0f, 1.0f, 0.0f));
}
