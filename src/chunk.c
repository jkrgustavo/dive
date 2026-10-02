#include "chunk.h"
#include "mat_math.h"

// dynamic scratch to find the best buffer size for each chunk
static struct {
    size_t v_cap;
    size_t v_n;
    float3 *v_data;

    size_t i_cap;
    size_t i_n;
    u32 *i_data;
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
        float3 *v_data = realloc(scratch.v_data, scratch.v_cap * sizeof(float3));
        assert(v_data);
        scratch.v_data = v_data;
    }
    scratch.v_data[scratch.v_n++] = v;
}

static void scratch_push_ind(u32 i) {
    if (scratch.i_n == scratch.i_cap) {
        scratch.i_cap = scratch.i_cap ? scratch.i_cap * 2 : 8192;
        u32 *i_data = realloc(scratch.i_data, scratch.i_cap * sizeof(u32));
        assert(i_data);
        scratch.i_data = i_data;
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

static const u32 cube_indices[] = {
    0, 3, 2,  0, 2, 1,   // -z
    4, 5, 6,  4, 6, 7,   // +z
    0, 4, 7,  0, 7, 3,   // -x
    1, 2, 6,  1, 6, 5,   // +x
    0, 1, 5,  0, 5, 4,   // -y
    3, 7, 6,  3, 6, 2    // +y
};

void chunk_alloc(struct Chunk *chunk) {
    assert(!chunk->flags.allocated);
    u8 *data = calloc(CHUNK_VOLUME, sizeof(u8));
    assert(data);
    chunk->data = data;
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

        u32 base_index = scratch.v_n;

        float3 pos = simd_float(index_to_local(vox));
        for (u32 v = 0; v < 8; v++) {
           scratch_push_vert(pos + cube_vert_mask[v]);
        }

        for (u32 i = 0; i < 36; i++) {
            scratch_push_ind(base_index + cube_indices[i]);
        }
    }

    if (scratch.v_n == 0) {
        chunk->flags.meshed = true;
        chunk->flags.meshing = false;
        return;
    }

    chunk->mesh.vertex_count = scratch.v_n;
    chunk->mesh.index_count = scratch.i_n;

    chunk->mesh.vbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.vertex_buffer = true,
        .data = { 
            .ptr = scratch.v_data, 
            .size = scratch.v_n * sizeof(float3)
        }
    });

    chunk->mesh.ibuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.index_buffer = true,
        .data = { 
            .ptr = scratch.i_data, 
            .size = scratch.i_n * sizeof(u32)
        }
    });
    
    chunk->flags.meshing = false;
    chunk->flags.meshed = true;
}

void chunk_render(const struct Chunk *chunk, simd_float4x4 proj_view, double3 cam_pos) {
    assert(chunk->flags.meshed);
    if (chunk->mesh.index_count == 0) return;

    double3 chunk_pos = voxel_to_world(chunk_to_voxel(chunk->chunk_pos));
    double3 rel_pos_d = chunk_pos - cam_pos;
    float3 rel_pos = simd_float(rel_pos_d);
    float3 scale = simd_make_float3(1.0, 1.0, 1.0);

    simd_float4x4 model_mat = simd_mul(mat_translation(rel_pos), mat_scale(scale));
    simd_float4x4 mvp = simd_mul(proj_view, model_mat);

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
    chunk->flags.meshing = false;
    chunk->chunk_pos = simd_make_int3(0, 0, 0);

    chunk->mesh.index_count = 0;
    chunk->mesh.vertex_count = 0;
    sg_destroy_buffer(chunk->mesh.vbuf);
    sg_destroy_buffer(chunk->mesh.ibuf);
    chunk->mesh.vbuf = (sg_buffer){0};
    chunk->mesh.ibuf = (sg_buffer){0};

    memset(chunk->data, 0, sizeof(u8) * CHUNK_VOLUME);
}

void chunk_destroy(struct Chunk *chunk) {
    chunk->flags.allocated = false;
    free(chunk->data);

    chunk->mesh.index_count = 0;
    chunk->mesh.vertex_count = 0;
    sg_destroy_buffer(chunk->mesh.vbuf);
    sg_destroy_buffer(chunk->mesh.ibuf);

    memset(chunk, 0, sizeof(struct Chunk));
}
