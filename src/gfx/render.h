#ifndef RENDER_H
#define RENDER_H

#include "utils/util.h"
#include "chunk_mesh.h"

struct Renderer {
    sg_pass_action pass_action;
    sg_shader shader;
    sg_pipeline pipeline_opaque;
    sg_pipeline pipeline_trans;

    // contains handles to all needed buffers
    sg_bindings bindings;

    struct {
        sg_buffer buf;
        sg_view view;
    } palette;

    simd_float4x4 proj_view;
};

// returns true on success, false if something went wrong
bool renderer_init(struct Renderer *renderer);
void renderer_frame(struct Renderer *renderer, simd_float4x4 cam_proj, simd_float4x4 cam_view,
        const struct ChunkDraw *draws, u32 count);
void renderer_reload(struct Renderer *renderer);
void renderer_destroy(struct Renderer *renderer);

#endif /* RENDER_H */
