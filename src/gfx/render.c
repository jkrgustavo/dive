#include "render.h"
#include "utils/util.h"
#include "utils/mat_math.h"

enum UniformBlock {
    UB_FRAME = 0,
    UB_CHUNK
};

enum ViewSlot {
    VIEW_QUADS = 0,
    VIEW_VOXELS,
    VIEW_PALETTE,
};

bool renderer_init(struct Renderer *renderer) {

    u32 palette[256];
    palette_colors(palette);
    renderer->palette.buf = sg_make_buffer(&(sg_buffer_desc) {
        .data = SG_RANGE(palette),
        .usage.storage_buffer = true
    });
    renderer->palette.view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = renderer->palette.buf
    });

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
        .uniform_blocks[UB_FRAME] = {
            .size = sizeof(simd_float4x4),
            .stage = SG_SHADERSTAGE_VERTEX,
            .msl_buffer_n = 0
        },
        .uniform_blocks[UB_CHUNK] = {
            .size = sizeof(simd_float4x4),
            .stage = SG_SHADERSTAGE_VERTEX,
            .msl_buffer_n = 1
        },
        .views[VIEW_QUADS].storage_buffer = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .readonly = true,
            .msl_buffer_n = 8
        },
        .views[VIEW_VOXELS].storage_buffer = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .readonly = true,
            .msl_buffer_n = 8
        },
        .views[VIEW_PALETTE].storage_buffer = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .readonly = true,
            .msl_buffer_n = 9
        }
    });

    sg_pipeline pipeline_opaque = sg_make_pipeline(&(sg_pipeline_desc){
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

    sg_pipeline pipeline_trans = sg_make_pipeline(&(sg_pipeline_desc){
        .label = "Voxel",
        .shader = shader,
        .index_type = SG_INDEXTYPE_NONE,
        .depth = { .compare = SG_COMPAREFUNC_GREATER_EQUAL, .write_enabled = false, },
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
        },
        .cull_mode = SG_CULLMODE_NONE,
    });
    free(vs_buffer);
    free(fs_buffer);

    bool shader_valid = sg_query_shader_state(shader) == SG_RESOURCESTATE_VALID;
    bool pipe_opaque_valid = sg_query_pipeline_state(pipeline_opaque) == SG_RESOURCESTATE_VALID;
    bool pipe_trans_valid = sg_query_pipeline_state(pipeline_trans) == SG_RESOURCESTATE_VALID;

    if (!shader_valid || !pipe_opaque_valid || !pipe_trans_valid) {
        sg_destroy_shader(shader);
        sg_destroy_pipeline(pipeline_opaque);
        sg_destroy_pipeline(pipeline_trans);
        return false;
    }

    renderer->shader = shader;
    renderer->pipeline_opaque = pipeline_opaque;
    renderer->pipeline_trans = pipeline_trans;

    return true;
}

static void draw_chunks_opaque(struct Renderer *renderer, const struct ChunkDraw *draws, u32 count) {
    sg_apply_pipeline(renderer->pipeline_opaque);
    sg_apply_uniforms(UB_FRAME, &SG_RANGE(renderer->proj_view));
    float3 scale = simd_make_float3(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE);
    for (u32 i = 0; i < count; i++) {
        const struct MeshBuffer *mesh = draws[i].mesh;
        if (mesh->opaque_count == 0) continue;
        simd_float4x4 model_mat = simd_mul(mat_translation(draws[i].rel_pos), mat_scale(scale));
        sg_apply_bindings(&(sg_bindings) {
            .views = {
                [VIEW_QUADS] = mesh->quad_view,
                [VIEW_VOXELS] = mesh->padded_view,
                [VIEW_PALETTE] = renderer->palette.view
            }
        });
        sg_apply_uniforms(UB_CHUNK, &SG_RANGE(model_mat));
        sg_draw(0, mesh->opaque_count * 6, 1);
    }
}

static void draw_chunks_translucent(struct Renderer *renderer, const struct ChunkDraw *draws, u32 count) {
    sg_apply_pipeline(renderer->pipeline_trans);
    sg_apply_uniforms(UB_FRAME, &SG_RANGE(renderer->proj_view));
    float3 scale = simd_make_float3(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE);
    for (u32 i = count; i-- > 0;) {
        const struct MeshBuffer *mesh = draws[i].mesh;
        if (mesh->trans_count == 0) continue;
        simd_float4x4 model_mat = simd_mul(mat_translation(draws[i].rel_pos), mat_scale(scale));
        sg_apply_bindings(&(sg_bindings) {
            .views = {
                [VIEW_QUADS] = mesh->quad_view,
                [VIEW_VOXELS] = mesh->padded_view,
                [VIEW_PALETTE] = renderer->palette.view
            }
        });
        sg_apply_uniforms(UB_CHUNK, &SG_RANGE(model_mat));
        sg_draw(mesh->opaque_count * 6, mesh->trans_count * 6, 1);
    }
}

void renderer_frame(struct Renderer *renderer, simd_float4x4 cam_proj, simd_float4x4 cam_view,
        const struct ChunkDraw *draws, u32 count
) {
    renderer->proj_view = simd_mul(cam_proj, cam_view);
    sg_begin_pass(&(sg_pass){ .action = renderer->pass_action, .swapchain = sglue_swapchain() });

    draw_chunks_opaque(renderer, draws, count);
    draw_chunks_translucent(renderer, draws, count);

    simgui_render();
    sg_end_pass();
    sg_commit();
}
 
void renderer_reload(struct Renderer *renderer) {
    sg_destroy_view(renderer->palette.view);
    sg_destroy_buffer(renderer->palette.buf);

    sg_pipeline old_pipeline_opaque = renderer->pipeline_opaque;
    sg_pipeline old_pipeline_trans = renderer->pipeline_trans;
    sg_shader old_shader = renderer->shader;

    if (!renderer_init(renderer)) {
        fprintf(stderr, "Error creating pipeline/shader, reusing older version.");
        renderer->pipeline_opaque = old_pipeline_opaque;
        renderer->pipeline_trans = old_pipeline_trans;
    } else {
        sg_destroy_shader(old_shader);
        sg_destroy_pipeline(old_pipeline_opaque);
        sg_destroy_pipeline(old_pipeline_trans);
    }

}

void renderer_destroy(struct Renderer *renderer) {
    sg_destroy_shader(renderer->shader);
    sg_destroy_pipeline(renderer->pipeline_opaque);
    sg_destroy_pipeline(renderer->pipeline_trans);

    sg_destroy_view(renderer->palette.view);
    sg_destroy_buffer(renderer->palette.buf);
}
