#include "render.h"

void renderer_init(struct Renderer *renderer) {
    renderer->pass_action = (sg_pass_action) {
        .colors[0] = {
            .clear_value = { 0.6, 0.5, 0.7, 1.0 },
            .load_action = SG_LOADACTION_CLEAR,
        }
    };

    char vshader[1024];
    char fshader[1024];
    assert(read_file(vshader, 1024, "src/shaders/vert.metal"));
    assert(read_file(fshader, 1024, "src/shaders/frag.metal"));
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .label = "Voxel",
        .vertex_func.source = vshader,
        .fragment_func.source = fshader,
        .uniform_blocks[0] = {
            .size = sizeof(simd_float4x4),
            .stage = SG_SHADERSTAGE_VERTEX,
            .msl_buffer_n = 0
        }
    });

    renderer->pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .label = "Voxel",
        .shader = shd,
        .layout.attrs = {
            [0] = { .format = SG_VERTEXFORMAT_FLOAT4 },
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = { 
            .compare = SG_COMPAREFUNC_LESS_EQUAL, 
            .write_enabled = true, 
        },
    });

    assert(sg_query_shader_state(shd) == SG_RESOURCESTATE_VALID);
    assert(sg_query_pipeline_state(renderer->pipeline) == SG_RESOURCESTATE_VALID);
}

void renderer_begin_pass(struct Renderer *renderer) {
    sg_begin_pass(&(sg_pass){ .action = renderer->pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(renderer->pipeline);
}

void renderer_end_pass() {
    simgui_render();
    sg_end_pass();
    sg_commit();
}
