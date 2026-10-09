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

static void mesh_release(struct MeshBuffer *mesh) {
    sg_destroy_view(mesh->vbuf_view);
    sg_destroy_buffer(mesh->vbuf);
    *mesh = (struct MeshBuffer){0};
}

// Takes in this chunk's world position in chunks
void chunk_init(struct Chunk *chunk, int3 chunk_pos) {
    assert(chunk);
    assert(!chunk->flags.initialized);
    if (!chunk->data) {
        u8 *data = calloc(CHUNK_VOLUME, sizeof(u8));
        assert(data);
        chunk->data = data;
    }

    chunk->position = chunk_pos;
    chunk->flags.initialized = true;
    chunk->flags.meshing = false;
    chunk->flags.meshed = false;
    chunk->flags.dirty = false;
}

void chunk_set_block(struct Chunk *chunk, int3 local_pos, enum VoxelType id) {
    assert(chunk->flags.initialized);
    u32 idx = local_to_chunk_index(local_pos);
    chunk->data[idx] = id;
    chunk->flags.dirty = true;
}

bool face_exposed(struct Chunk *chunk, const struct Chunk *neighbors[6], int3 local_pos, u32 dir) {
    int3 neighbor_pos = local_pos + dir_offset[dir];

    if (!voxel_in_chunk(neighbor_pos)) {
        const struct Chunk *neighbor_chunk = neighbors[dir];

        if (neighbor_chunk == NULL) return true;
        if (!neighbor_chunk->flags.initialized) return true;

        int3 neighbor_pos_abs = local_to_voxel(local_pos + dir_offset[dir], chunk->position);
        u32 neighbor_vox_idx = local_to_chunk_index(voxel_to_local(neighbor_pos_abs));
        
        return neighbor_chunk->data[neighbor_vox_idx] == AIR;
    } else {
        return chunk->data[local_to_chunk_index(neighbor_pos)] == AIR;
    }
}

void chunk_mesh(struct Chunk *chunk, const struct Chunk *neighbors[6]) {
    assert(chunk && chunk->flags.initialized && !chunk->flags.meshing);
    if (chunk->flags.meshed && !chunk->flags.dirty)
        return;

    chunk->flags.meshing = true;
    mesh_release(&chunk->mesh);

    scratch.v_n = 0;
    for (size_t vox = 0; vox < CHUNK_VOLUME; vox++) {
        if (chunk->data[vox] == AIR)
            continue;

        int3 local_pos = chunk_index_to_local(vox);
        for (u32 dir = 0; dir < 6; dir++) {
            if (!face_exposed(chunk, neighbors, local_pos, dir))
                continue;

            u64 quad = pack_quad(local_pos + face_vertex_mask[dir], 1, 1, dir, chunk->data[vox]);
            scratch_push_vert(quad);
        }
    }

    if (scratch.v_n == 0) {
        mesh_release(&chunk->mesh);
        chunk->flags.meshing = false;
        chunk->flags.dirty = false;
        chunk->flags.meshed = true;
        return;
    }

    chunk->mesh.vertex_count = scratch.v_n;

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
    
    chunk->flags.meshing = false;
    chunk->flags.dirty = false;
    chunk->flags.meshed = true;
}

void chunk_render(const struct Chunk *chunk, sg_buffer ibuf, double3 cam_pos) {
    assert(chunk);
    assert(chunk->flags.meshed);
    if (chunk->mesh.vertex_count == 0) return;

    double3 chunk_pos = voxel_to_world(chunk_to_voxel(chunk->position));
    double3 rel_pos_d = chunk_pos - cam_pos;
    float3 rel_pos = simd_float(rel_pos_d);
    float3 scale = simd_make_float3(1.0, 1.0, 1.0);

    simd_float4x4 model_mat = simd_mul(mat_translation(rel_pos), mat_scale(scale));

    sg_apply_bindings(&(sg_bindings){
        .index_buffer = ibuf,
        .views[0] = chunk->mesh.vbuf_view
    });
    // view and proj mats are applied in the begin_pass function
    sg_apply_uniforms(1, &SG_RANGE(model_mat));
    sg_draw(0, chunk->mesh.vertex_count * 6, 1);
}

void chunk_reset(struct Chunk *chunk) {
    assert(chunk);
    chunk->flags.initialized = false;
    chunk->flags.meshed = false;
    chunk->flags.meshing = false;
    chunk->flags.dirty = false;
    chunk->position = simd_make_int3(0, 0, 0);

    chunk->mesh.vertex_count = 0;
    mesh_release(&chunk->mesh);
    memset(chunk->data, 0, sizeof(u8) * CHUNK_VOLUME);
}

void chunk_destroy(struct Chunk *chunk) {
    assert(chunk);
    free(chunk->data);

    chunk->mesh.vertex_count = 0;
    sg_destroy_buffer(chunk->mesh.vbuf);
    sg_destroy_view(chunk->mesh.vbuf_view);

    memset(chunk, 0, sizeof(struct Chunk));
}
