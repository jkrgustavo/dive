#include "render.h"

void renderer_init(struct Renderer *renderer) {
    renderer->pass_action = (sg_pass_action) {
        .colors[0] = {
            .clear_value = { 0.6, 0.5, 0.7, 1.0 },
            .load_action = SG_LOADACTION_CLEAR,
        }
    };

    char *vs_buffer = read_file("src/shaders/vert.metal");
    assert(vs_buffer);
    char *fs_buffer = read_file("src/shaders/frag.metal");
    assert(fs_buffer);
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .label = "Voxel",
        .vertex_func.source = vs_buffer,
        .fragment_func.source = fs_buffer,
        .uniform_blocks[0] = {
            .size = sizeof(simd_float4x4),
            .stage = SG_SHADERSTAGE_VERTEX,
            .msl_buffer_n = 0
        },
        .uniform_blocks[1] = {
            .size = sizeof(simd_float4x4),
            .stage = SG_SHADERSTAGE_VERTEX,
            .msl_buffer_n = 1
        },
        .views[0].storage_buffer = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .readonly = true,
            .msl_buffer_n = 8
        }
    });

    renderer->pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .label = "Voxel",
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT32,
        .depth = { 
            .compare = SG_COMPAREFUNC_LESS_EQUAL, 
            .write_enabled = true, 
        },
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
    });

    assert(sg_query_shader_state(shd) == SG_RESOURCESTATE_VALID);
    assert(sg_query_pipeline_state(renderer->pipeline) == SG_RESOURCESTATE_VALID);
    free(vs_buffer);
    free(fs_buffer);
}

void renderer_begin_pass(struct Renderer *renderer, const struct Camera *camera) {
    sg_begin_pass(&(sg_pass){ .action = renderer->pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(renderer->pipeline);
    simd_float4x4 proj_view = simd_mul(camera->proj, camera->view);
    sg_apply_uniforms(0, &SG_RANGE(proj_view));
}

void renderer_end_pass() {
    simgui_render();
    sg_end_pass();
    sg_commit();
}
