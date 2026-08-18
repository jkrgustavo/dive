#include "chunk.h"
#include "math.h"

// dynamic scratch to find the best buffer size for each chunk
static struct {
    size_t v_cap;
    size_t v_n;
    float3 *v_data;

    size_t i_cap;
    size_t i_n;
    u16 *i_data;
} scratch = {
    .v_cap = 0,
    .v_n = 0,
    .v_data = NULL,

    .i_cap = 0,
    .i_n = 0,
    .i_data = NULL,
};

static void scratch_push_vert(float3 v) {
    if (scratch.v_n == scratch.v_cap) {
        scratch.v_cap = scratch.v_cap ? scratch.v_cap * 2 : 8192;
        scratch.v_data = realloc(scratch.v_data, scratch.v_cap * sizeof(float3));
    }
    scratch.v_data[scratch.v_n++] = v;
}

static void scratch_push_ind(u16 i) {
    if (scratch.i_n == scratch.i_cap) {
        scratch.i_cap = scratch.i_cap ? scratch.i_cap * 2 : 8192;
        scratch.i_data = realloc(scratch.i_data, scratch.i_cap * sizeof(u16));
    }
    scratch.i_data[scratch.i_n++] = i;
}

static const float3 cube_vert_mask[] = {
    { 0.0, 0.0, 0.0 },
    { 1.0, 0.0, 0.0 },
    { 1.0, 1.0, 0.0 },
    { 0.0, 1.0, 0.0 },

    { 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 1.0 },
    { 1.0, 1.0, 1.0 },
    { 0.0, 1.0, 1.0 },
};

static const u16 cube_indices[] = {
    0, 3, 2,  0, 2, 1,   // -z
    4, 5, 6,  4, 6, 7,   // +z
    0, 4, 7,  0, 7, 3,   // -x
    1, 2, 6,  1, 6, 5,   // +x
    0, 1, 5,  0, 5, 4,   // -y
    3, 7, 6,  3, 6, 2    // +y
};

void chunk_alloc(struct Chunk *chunk) {
    chunk->data = calloc(1, sizeof(u8) * CHUNK_VOLUME);
    chunk->flags.allocated = true;
}

void chunk_init(struct Chunk *chunk, int3 chunk_pos) {
    assert(chunk->flags.allocated && !chunk->flags.initialized);
    chunk->chunk_pos = chunk_pos;
    chunk->flags.initialized = true;
}
// 6 faces, 8 verts, 36 indices
// TODO: use quads instead of the entire voxel
void chunk_mesh(struct Chunk *chunk) {
    assert(chunk->flags.allocated && chunk->flags.initialized && !chunk->flags.meshing);
    if (chunk->flags.meshed)
        return;

    chunk->flags.meshing = true;

    scratch.i_n = 0;
    scratch.v_n = 0;
    for (size_t vox = 0; vox < CHUNK_VOLUME; vox++) {
        if (chunk->data[vox] == AIR) {
            continue;
        }

        u16 base_index = scratch.v_n;

        float3 pos = simd_float(index_to_local(vox));
        for (u32 v = 0; v < 8; v++) {
           scratch_push_vert(pos + cube_vert_mask[v]);
        }

        for (u32 i = 0; i < 36; i++) {
            scratch_push_ind(base_index + cube_indices[i]);
        }
    }

    chunk->mesh.vertex_count = scratch.v_n;
    chunk->mesh.index_count = scratch.i_n;
    chunk->mesh.vertices = malloc(sizeof(float3) * scratch.v_n);
    chunk->mesh.indices = malloc(sizeof(u16) * scratch.i_n);

    memcpy(chunk->mesh.vertices, scratch.v_data, sizeof(float3) * scratch.v_n);
    memcpy(chunk->mesh.indices, scratch.i_data, sizeof(u16) * scratch.i_n);

    chunk->mesh.vbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.vertex_buffer = true,
        .data = { 
            .ptr = chunk->mesh.vertices, 
            .size = chunk->mesh.vertex_count * sizeof(float3)
        }
    });

    chunk->mesh.ibuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.index_buffer = true,
        .data = { 
            .ptr = chunk->mesh.indices, 
            .size = chunk->mesh.index_count * sizeof(u16)
        }
    });
    
    chunk->flags.meshing = false;
    chunk->flags.meshed = true;
}

void chunk_render(struct Chunk *chunk, struct Camera *cam) {
    assert(chunk->flags.meshed);

    double3 chunk_pos = voxel_to_world(chunk_to_voxel(chunk->chunk_pos));
    double3 cam_pos = cam->pos;
    double3 rel_pos_d = chunk_pos - cam_pos;
    float3 rel_pos = simd_float(rel_pos_d);
    float3 scale = simd_make_float3(1.0, 1.0, 1.0);

    simd_float4x4 model_mat = simd_mul(mat_scale(scale), mat_translation(rel_pos));
    simd_float4x4 mvp = simd_mul(cam->proj, simd_mul(cam->view, model_mat));

    sg_apply_bindings(&(sg_bindings){
        .index_buffer = chunk->mesh.ibuf,
        .vertex_buffers[0] = chunk->mesh.vbuf
    });

    sg_apply_uniforms(0, &SG_RANGE(mvp));
    sg_draw(0, chunk->mesh.index_count, 1);
}

void chunk_uninit(struct Chunk *chunk) {
    chunk->flags.initialized = false;
    chunk->flags.meshed = false;
    chunk->chunk_pos = simd_make_int3(0, 0, 0);

    chunk->mesh.index_count = 0;
    chunk->mesh.vertex_count = 0;
    free(chunk->mesh.vertices);
    free(chunk->mesh.indices);
    chunk->mesh.vertices = NULL;
    chunk->mesh.indices = NULL;

    memset(chunk->data, 0, sizeof(u8) * CHUNK_VOLUME);
}

void chunk_destroy(struct Chunk *chunk) {
    chunk->flags.allocated = false;
    free(chunk->data);

    if (chunk->flags.meshed) {
        chunk->mesh.index_count = 0;
        chunk->mesh.vertex_count = 0;
        free(chunk->mesh.vertices);
        free(chunk->mesh.indices);
        sg_destroy_buffer(chunk->mesh.vbuf);
        sg_destroy_buffer(chunk->mesh.ibuf);
    }

    memset(chunk, 0, sizeof(struct Chunk));
}
