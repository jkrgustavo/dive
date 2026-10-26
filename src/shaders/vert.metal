#include <metal_stdlib>

using namespace metal;

struct mats {
    float4x4 mvp;
};

struct vs_out {
    float4 position [[position]];
    float3 local_pos;       // chunk_local, in voxels
    float3 normal [[flat]]; // face dir, const across the quad
};


constant float3 dir_offset[] = {
    {  0,  0,  1 }, // front +z
    {  0,  0, -1 }, // back  -z
    {  1,  0,  0 }, // left  +x
    { -1,  0,  0 }, // right -x
    {  0,  1,  0 }, // up    +y
    {  0, -1,  0 }, // down  -y
};

// front (+z), back (-z), left (+x), right (-x), up (+y), down (-y)
constant float3 corners[6][4] = {
    { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} },
    { {0,0,0}, {0,1,0}, {1,1,0}, {1,0,0} },
    { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} },
    { {0,0,0}, {0,0,1}, {0,1,1}, {0,1,0} },
    { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} },
    { {0,0,0}, {1,0,0}, {1,0,1}, {0,0,1} },
};

vertex vs_out _main(
    uint vid [[vertex_id]], 
    device const float4x4& view_proj_mat [[buffer(0)]],
    device const float4x4& model_mat [[buffer(1)]],
    device const uint64_t* quads [[buffer(8)]] 
) {
    uint face = vid >> 2;
    uint corner = vid & 3u;

    uint64_t q = quads[face];
    float3 pos = float3(uint3(q & 63u, (q>>6) & 63u, (q>>12) & 63u));
    uint dir = uint((q>>30) & 7u);
    float3 corner_val = corners[dir][corner];

    vs_out out;
    out.position = view_proj_mat * model_mat * float4(pos + corner_val, 1.0);
    out.local_pos = pos + corner_val;
    out.normal = dir_offset[dir];
    return out;
}
