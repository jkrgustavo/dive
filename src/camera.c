#include "camera.h"
#include "math.h"

void camera_init(struct Camera *cam, float fovy, float near, float far, float aspect) {
    cam->pos     = simd_make_double3(2.0, 2.0, -10.0);
    cam->forward = simd_make_float3(0.0, 0.0, 1.0);
    cam->up     = simd_make_float3(0.0, 1.0, 0.0);
    cam->near = near;
    cam->far = far;
    cam->fovy = fovy;
    cam->aspect = aspect;

    cam->view = mat_view_dir(cam->forward, cam->up);
    cam->proj = mat_proj(cam->fovy, cam->aspect, cam->near, cam->far);
}

void camera_update(struct Camera *cam, float3 forward) {
    cam->forward = forward;
    cam->view = mat_view_dir(cam->forward, cam->up);
}
