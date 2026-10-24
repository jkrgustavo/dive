#include "chunk.h"
#include "utils/mat_math.h"
#include <ba_buffer.h>
#include <simd/geometry.h>

#define PADDED_CHUNK_DIM_X (CHUNK_DIM_X+2)
#define PADDED_CHUNK_DIM_Y (CHUNK_DIM_Y+2)
#define PADDED_CHUNK_DIM_Z (CHUNK_DIM_Z+2)

#define PADDED_CHUNK_VOLUME ((PADDED_CHUNK_DIM_X) * (PADDED_CHUNK_DIM_Y) * (PADDED_CHUNK_DIM_Z))

static const int3 face_vertex_mask[] = {
    { 0, 0, 1 },  // front
    { 0, 0, 0 },  // back
    { 1, 0, 0 },  // left
    { 0, 0, 0 },  // right
    { 0, 1, 0 },  // up
    { 0, 0, 0 },  // down
};

static u64 pack_quad(int3 p, u8 width, u8 height, u8 dir) {
    return  (u64)p.x
         | ((u64)p.y    << 6)
         | ((u64)p.z    << 12)
         | ((u64)width  << 18)
         | ((u64)height << 24)
         | ((u64)dir    << 30);
}

static void mesh_release(struct MeshBuffer *mesh) {
    sg_destroy_view(mesh->vbuf_view);
    sg_destroy_buffer(mesh->vbuf);

    sg_destroy_view(mesh->paddbuf_view);
    sg_destroy_buffer(mesh->paddbuf);
    mesh->vertex_count = 0;

    *mesh = (struct MeshBuffer){0};
}

// Takes in this chunk's world position in chunks
void chunk_init(struct Chunk *chunk, int3 chunk_pos) {
    bs_assert(chunk, "Don't init a loaded chunk");
    bs_assert(!chunk->flags.initialized, "Chunk must be uninitialized before calling init");
    if (!chunk->data) {
        u8 *data = calloc(CHUNK_VOLUME, sizeof(u8));
        bs_assert(data, "OOM while initing chunk");
        chunk->data = data;
    }

    // init flag set after terrain gen
    chunk->position = chunk_pos;
    chunk->flags.meshing = false;
    chunk->flags.meshed = false;
    chunk->flags.empty = false;
    chunk->flags.dirty = false;
    chunk->flags.initialized = true;
}

void chunk_set_block(struct Chunk *chunk, int3 local_pos, enum VoxelColor id) {
    bs_assert(chunk->flags.initialized, "No setting blocks on an uninit chunk");
    u32 idx = local_to_chunk_index(local_pos);
    chunk->data[idx] = id;
    chunk->flags.dirty = true;
}

static void build_padded_data(u8 *padded, const struct Chunk *neighbors[27]) {
    // triple loop bc padded chunk dim isn't a power of two
    for (i32 z = -1; z < PADDED_CHUNK_DIM_Z-1; z++) {
        for (i32 y = -1; y < PADDED_CHUNK_DIM_Y-1; y++) {
            for (i32 x = -1; x < PADDED_CHUNK_DIM_X-1; x++) {
                int3 vox = simd_make_int3(x, y, z);
                int3 offset = voxel_to_chunk(vox) + 1;
                const struct Chunk *neighbor_chunk = neighbors[offset.x + offset.y*3 + offset.z*3*3];
                u8 vox_data;
                if (!neighbor_chunk) { 
                    vox_data = INVALID; 
                } else if (neighbor_chunk->flags.empty) {
                    vox_data = AIR;
                } else {
                    vox_data = neighbor_chunk->data[local_to_chunk_index(voxel_to_local(vox))];
                }
                u32 padded_idx = (x+1) + (y+1) * PADDED_CHUNK_DIM_X + (z+1) * (PADDED_CHUNK_DIM_X * PADDED_CHUNK_DIM_Y);
                padded[padded_idx] = vox_data;
            }
        }
    }
}

static const i32 padded_stride[6] = {
     34 * 34,  // FRONT +z
    -34 * 34,  // BACK  -z
     1,        // LEFT  +x
    -1,        // RIGHT -x
     34,       // UP    +y
    -34,       // DOWN  -y
};

void chunk_mesh(struct Chunk *chunk, const struct Chunk *neighbors[27], struct MeshScratch *scratch) {
    bs_assert(chunk && chunk->flags.initialized && !chunk->flags.meshing, "Invalid chunk");
    if ((chunk->flags.meshed && !chunk->flags.dirty) || chunk->flags.empty)
        return;

    chunk->flags.meshing = true;
    mesh_release(&chunk->mesh);
    build_padded_data(scratch->padded_data, neighbors);

    bs_buffer_clear(&scratch->quads);

    bool solid = false;
    for (size_t vox = 0; vox < CHUNK_VOLUME; vox++) {
        if (chunk->data[vox] == AIR)
            continue;

        int3 local_pos = chunk_index_to_local(vox);
        i32 vox_padded_idx = (local_pos.x+1) + (local_pos.y+1) * 34 + (local_pos.z+1) * 34 * 34;

        solid = true;
        for (u32 dir = 0; dir < 6; dir++) {
            if (scratch->padded_data[vox_padded_idx + padded_stride[dir]] != AIR)
                continue;

            u64 quad = pack_quad(local_pos + face_vertex_mask[dir], 1, 1, dir);
            bs_buffer_append(&scratch->quads, quad);
        }
    }

    chunk->mesh.vertex_count = scratch->quads.count;

    if (scratch->quads.count == 0) {
        chunk->flags.empty = !solid;
        if (chunk->flags.empty) {
            free(chunk->data);
            chunk->data = NULL;
        }
        chunk->flags.meshing = false;
        chunk->flags.dirty = false;
        chunk->flags.meshed = true;
        return;
    }

    chunk->mesh.vbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { .ptr = scratch->quads.items, .size = scratch->quads.count * sizeof(u64) }
    });
    chunk->mesh.vbuf_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = chunk->mesh.vbuf,
    });


    chunk->mesh.paddbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { .ptr = scratch->padded_data, .size = PADDED_CHUNK_VOLUME * sizeof(u8) }
    });
    chunk->mesh.paddbuf_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = chunk->mesh.paddbuf
    });
    
    chunk->flags.meshing = false;
    chunk->flags.dirty = false;
    chunk->flags.meshed = true;
}

void chunk_render(const struct Chunk *chunk, sg_buffer ibuf, sg_view palette, double3 cam_pos) {
    assert(chunk);
    assert(chunk->flags.meshed);
    if (chunk->mesh.vertex_count == 0) return;

    double3 chunk_pos = voxel_to_world(chunk_to_voxel(chunk->position));
    double3 rel_pos_d = chunk_pos - cam_pos;
    float3 rel_pos = simd_float(rel_pos_d);
    float3 scale = simd_make_float3(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE);

    simd_float4x4 model_mat = simd_mul(mat_translation(rel_pos), mat_scale(scale));

    sg_apply_bindings(&(sg_bindings){
        .index_buffer = ibuf,
        .views = { 
            [0] = chunk->mesh.vbuf_view, 
            [1] = chunk->mesh.paddbuf_view,
            [2] = palette
        }
    });
    // view and proj mats are applied in the begin_pass function
    sg_apply_uniforms(1, &SG_RANGE(model_mat));
    sg_draw(0, chunk->mesh.vertex_count * 6, 1);
}

void chunk_reset(struct Chunk *chunk) {
    assert(chunk);
    if (!chunk->flags.empty) {
        mesh_release(&chunk->mesh);
        memset(chunk->data, 0, sizeof(u8) * CHUNK_VOLUME);
    }

    chunk->flags.initialized = false;
    chunk->flags.meshed = false;
    chunk->flags.meshing = false;
    chunk->flags.dirty = false;
    chunk->flags.empty = false;
    chunk->position = simd_make_int3(0, 0, 0);
}

void chunk_destroy(struct Chunk *chunk) {
    assert(chunk);
    if (!chunk->flags.empty) {
        mesh_release(&chunk->mesh);
        free(chunk->data);
    }

    memset(chunk, 0, sizeof(struct Chunk));
}
