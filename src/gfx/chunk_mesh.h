#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include "utils/util.h"
#include "world/voxel.h"

struct MeshBuffer {
    u32 quad_count;

    sg_view quad_view;
    sg_buffer quads;

    sg_view padded_view;
    sg_buffer padded;
};

struct ChunkDraw {
    const struct MeshBuffer *mesh;
    float3 rel_pos;
};

struct MeshBuffer renderer_mesh_create(const u64 *quads, u32 quad_count, const voxel *padded, size_t padded_count);
void renderer_mesh_destroy(struct MeshBuffer *mesh);

#endif /* CHUNK_MESH_H */
