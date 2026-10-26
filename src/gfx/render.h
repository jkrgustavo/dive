#ifndef RENDER_H
#define RENDER_H

#include "utils/util.h"

struct Renderer {
    sg_pass_action pass_action;
    sg_pipeline pipeline;

    // contains handles to all needed buffers
    sg_bindings bindings;
};

void renderer_init(struct Renderer *renderer);
void renderer_begin_pass(struct Renderer *renderer, simd_float4x4 cam_proj, simd_float4x4 cam_view);
void renderer_end_pass();

#endif /* RENDER_H */
