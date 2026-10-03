#include "chunk.h"
#include "mat_math.h"

// dynamic scratch to find the best buffer size for each chunk
static struct {
    size_t v_cap;
    size_t v_n;
    u64 *v_data;

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

static void scratch_push_vert(u64 v) {
    if (scratch.v_n == scratch.v_cap) {
        scratch.v_cap = scratch.v_cap ? scratch.v_cap * 2 : 8192;
        u64 *v_data = realloc(scratch.v_data, scratch.v_cap * sizeof(u64));
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

static const u32 face_indices[] = { 0, 1, 2,  0, 2, 3 };
static const int3 face_vertex_mask[] = {
    { 0, 0, 1 },  // front
    { 0, 0, 0 },  // back
    { 1, 0, 0 },  // left
    { 0, 0, 0 },  // right
    { 0, 1, 0 },  // up
    { 0, 0, 0 },  // down
};

static u64 pack_quad(int3 p, u8 width, u8 height, u8 dir, u16 data) {
    return  (u64)p.x
         | ((u64)p.y    << 6)
         | ((u64)p.z    << 12)
         | ((u64)width  << 18)
         | ((u64)height << 24)
         | ((u64)dir    << 30)
         | ((u64)data   << 33);
}


void chunk_init(struct Chunk *chunk, int3 chunk_pos) {
    assert(chunk);
    memset(chunk, 0, sizeof(struct Chunk));

    u8 *data = calloc(CHUNK_VOLUME, sizeof(u8));
    assert(data);

    chunk->data = data;
    chunk->chunk_pos = chunk_pos;
    chunk->flags.initialized = true;
}
// 6 faces, 8 verts, 36 indices
// TODO: use quads instead of the entire voxel
void chunk_mesh(struct Chunk *chunk) {
    assert(chunk && chunk->flags.initialized && !chunk->flags.meshing);
    if (chunk->flags.meshed)
        return;

    chunk->flags.meshing = true;

    scratch.i_n = 0;
    scratch.v_n = 0;
    for (size_t vox = 0; vox < CHUNK_VOLUME; vox++) {
        if (chunk->data[vox] == AIR) {
            continue;
        }

        int3 local_pos = index_to_local(vox);
        for (u32 dir = 0; dir < 6; dir++) {
            u32 base_index = scratch.v_n * 4;
            scratch_push_vert(pack_quad(local_pos + face_vertex_mask[dir], 1, 1, dir, chunk->data[vox]));

            for (u32 idx = 0; idx < 6; idx++) {
                scratch_push_ind(base_index + face_indices[idx]);
            }
        }
    }

    if (scratch.v_n == 0) {
        chunk->flags.meshing = false;
        chunk->flags.meshed = true;
        return;
    }

    chunk->mesh.vertex_count = scratch.v_n;
    chunk->mesh.index_count = scratch.i_n;

    chunk->mesh.vbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { 
            .ptr = scratch.v_data, 
            .size = scratch.v_n * sizeof(u64)
        }
    });

    chunk->mesh.vbuf_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = chunk->mesh.vbuf,
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

void chunk_render(const struct Chunk *chunk, double3 cam_pos) {
    assert(chunk);
    assert(chunk->flags.meshed);
    if (chunk->mesh.index_count == 0) return;

    double3 chunk_pos = voxel_to_world(chunk_to_voxel(chunk->chunk_pos));
    double3 rel_pos_d = chunk_pos - cam_pos;
    float3 rel_pos = simd_float(rel_pos_d);
    float3 scale = simd_make_float3(1.0, 1.0, 1.0);

    simd_float4x4 model_mat = simd_mul(mat_translation(rel_pos), mat_scale(scale));

    sg_apply_bindings(&(sg_bindings){
        .index_buffer = chunk->mesh.ibuf,
        .views[0] = chunk->mesh.vbuf_view
    });

    sg_apply_uniforms(1, &SG_RANGE(model_mat));
    sg_draw(0, chunk->mesh.index_count, 1);
}

void chunk_reset(struct Chunk *chunk) {
    assert(chunk);
    chunk->flags.initialized = false;
    chunk->flags.meshed = false;
    chunk->flags.meshing = false;
    chunk->chunk_pos = simd_make_int3(0, 0, 0);

    chunk->mesh.index_count = 0;
    chunk->mesh.vertex_count = 0;
    sg_destroy_buffer(chunk->mesh.vbuf);
    sg_destroy_buffer(chunk->mesh.ibuf);
    sg_destroy_view(chunk->mesh.vbuf_view);
    chunk->mesh.vbuf = (sg_buffer){0};
    chunk->mesh.ibuf = (sg_buffer){0};

    memset(chunk->data, 0, sizeof(u8) * CHUNK_VOLUME);
}

void chunk_destroy(struct Chunk *chunk) {
    assert(chunk);
    free(chunk->data);

    chunk->mesh.index_count = 0;
    chunk->mesh.vertex_count = 0;
    sg_destroy_buffer(chunk->mesh.vbuf);
    sg_destroy_buffer(chunk->mesh.ibuf);
    sg_destroy_view(chunk->mesh.vbuf_view);

    memset(chunk, 0, sizeof(struct Chunk));
    chunk = NULL;
}
