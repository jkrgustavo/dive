#include <metal_stdlib>

using namespace metal;

struct mats {
    float4x4 mvp;
};

struct vs_out {
    float4 position [[position]];
    float4 color;
};

struct vs_in {
    float4 pos [[attribute(0)]];
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

vertex vs_out _main(
    vs_in in [[stage_in]], 
    uint vid [[vertex_id]], 
    constant mats& mat [[buffer(0)]]
) {
    vs_out out;
    out.position = mat.mvp * float4(in.pos.xyz, 1.0);
    out.color = float4(hash_color(vid), 1.0);
    return out;
}
