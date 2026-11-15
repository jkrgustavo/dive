#include "render.h"
#include "utils/util.h"

bool renderer_init(struct Renderer *renderer) {
    renderer->pass_action = (sg_pass_action) {
        .depth = {
            .clear_value = 0,
            .load_action = SG_LOADACTION_CLEAR
        },
        .colors[0] = {
            .clear_value = { 0.6, 0.5, 0.7, 1.0 },
            .load_action = SG_LOADACTION_CLEAR,
        }
    };

    char *vs_buffer = read_file("src/shaders/vert.metal");
    assert(vs_buffer);
    char *fs_buffer = read_file("src/shaders/frag.metal");
    assert(fs_buffer);
    sg_shader shader = sg_make_shader(&(sg_shader_desc){
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
        },
        .views[1].storage_buffer = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .readonly = true,
            .msl_buffer_n = 8
        },
        .views[2].storage_buffer = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .readonly = true,
            .msl_buffer_n = 9
        }
    });

    sg_pipeline pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .label = "Voxel",
        .shader = shader,
        .index_type = SG_INDEXTYPE_NONE,
        .depth = { 
            .compare = SG_COMPAREFUNC_GREATER_EQUAL, 
            .write_enabled = true, 
        },
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
    });
    free(vs_buffer);
    free(fs_buffer);

    bool shader_valid = sg_query_shader_state(shader) == SG_RESOURCESTATE_VALID;
    bool pipe_valid = sg_query_pipeline_state(pipeline) == SG_RESOURCESTATE_VALID;

    if (!shader_valid || !pipe_valid) {
        sg_destroy_shader(shader);
        sg_destroy_pipeline(pipeline);
        return false;
    }

    renderer->shader = shader;
    renderer->pipeline = pipeline;

    return true;
}

void renderer_begin_pass(struct Renderer *renderer, simd_float4x4 cam_proj, simd_float4x4 cam_view) {
    sg_begin_pass(&(sg_pass){ .action = renderer->pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(renderer->pipeline);
    simd_float4x4 proj_view = simd_mul(cam_proj, cam_view);
    sg_apply_uniforms(0, &SG_RANGE(proj_view));
}

void renderer_end_pass() {
    simgui_render();
    sg_end_pass();
    sg_commit();
}
 
void renderer_reload(struct Renderer *renderer) {
    sg_pipeline old_pipeline = renderer->pipeline;
    sg_shader old_shader = renderer->shader;

    if (!renderer_init(renderer)) {
        fprintf(stderr, "Error creating pipeline/shader, reusing older version.");
        renderer->shader = old_shader;
        renderer->pipeline = old_pipeline;
    } else {
        sg_destroy_shader(old_shader);
        sg_destroy_pipeline(old_pipeline);
    }

}

void renderer_destroy(struct Renderer *renderer) {
    sg_destroy_shader(renderer->shader);
    sg_destroy_pipeline(renderer->pipeline);
}
