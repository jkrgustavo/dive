#ifndef RENDER_H
#define RENDER_H

#include "util.h"
#include "camera.h"

struct Renderer {
    sg_pass_action pass_action;
    sg_pipeline pipeline;

    // contains handles to all needed buffers
    sg_bindings bindings;
};

void renderer_init(struct Renderer *renderer);
void renderer_begin_pass(struct Renderer *renderer, const struct Camera *camera);
void renderer_end_pass();

#endif /* RENDER_H */
