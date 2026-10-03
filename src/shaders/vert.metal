#include <metal_stdlib>

using namespace metal;

struct mats {
    float4x4 mvp;
};

struct vs_out {
    float4 position [[position]];
    float4 color;
};


static float3 hash_color(uint n) {
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n = n ^ (n >> 4);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15);
    return float3(float( n        & 255u),
                  float((n >>  8) & 255u),
                  float((n >> 16) & 255u)) / 255.0;
}

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
    constant float4x4& view_proj_mat [[buffer(0)]],
    constant float4x4& model_mat [[buffer(1)]],
    device const uint64_t* quads [[buffer(8)]] 
) {
    uint face = vid >> 2;
    uint corner = vid & 3u;

    uint64_t q = quads[face];
    float3 pos = float3(uint3(q & 63u, (q>>6) & 63u, (q>>12) & 63u));
    uint dir = uint((q>>30) & 7u);

    vs_out out;
    out.position = view_proj_mat * model_mat * float4(pos + corners[dir][corner], 1.0);
    out.color = float4(hash_color(vid), 1.0);
    return out;
}
