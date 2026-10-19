#include <metal_stdlib>

using namespace metal;

struct vs_in {
    float3 local_pos;       // chunk_local, in voxels
    float3 normal;          // face dir, const across the quad
};

fragment float4 _main(
    vs_in in [[stage_in]],
    constant uint8_t *voxel_dat [[buffer(8)]],
    constant uint *palette [[buffer(9)]]
) {
    int3 vox = int3(floor(in.local_pos - 0.5 * in.normal));
    vox = clamp(vox, int3(0), int3(31));
    uint8_t data = voxel_dat[vox.x + vox.y * 32 + vox.z * 1024];
    float3 color = unpack_unorm4x8_to_float(palette[data]).rgb;
    float light = 0.65 + 0.35 * saturate(dot(in.normal, normalize(float3(0.4, 1.0, 0.3))));
    return float4(color * light, 1.0);
}
