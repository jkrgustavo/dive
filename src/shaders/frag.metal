#include <metal_stdlib>

using namespace metal;

struct vs_in {
    float3 local_pos;       // chunk_local, in voxels
    float3 normal;          // face dir, const across the quad
};

bool is_solid(constant uint8_t *padded, int3 vox_pos) {
    vox_pos += 1;
    return padded[vox_pos.x + vox_pos.y*34 + vox_pos.z*34*34] != 0;
}

float corner_ao(constant uint8_t *padded, int3 a, int3 t1, int3 t2, int d1, int d2) {
    bool s1 = is_solid(padded, a + d1 * t1);
    bool s2 = is_solid(padded, a + d2 * t2);
    bool c  = is_solid(padded, a + (d2 * t2) + (d1 * t1));
    return (s1 && s2) ? 0.0 : 3.0 - float(int(s1) + int(s2) + int(c));

}

float get_light(constant uint8_t *padded, float3 vox, float3 norm) {
    int3 a  = int3(floor(vox + 0.5 * norm));             // air voxel in front of the face
    int3 t1 = abs(norm.x) > 0.5 ? int3(0, 1, 0) : int3(1, 0, 0);  // the face's two in-plane axes
    int3 t2 = abs(norm.z) > 0.5 ? int3(0, 1, 0) : int3(0, 0, 1);
    float2 f = float2(dot(fract(vox), float3(t1)), dot(fract(vox), float3(t2)));
    float lo = mix(corner_ao(padded, a, t1, t2, -1, -1), corner_ao(padded, a, t1, t2, 1, -1), f.x);
    float hi = mix(corner_ao(padded, a, t1, t2, -1,  1), corner_ao(padded, a, t1, t2, 1,  1), f.x);
    float ao = mix(lo, hi, f.y) / 3.0;                                  // 0 = fully occluded, 1 = open
    float sun = saturate(dot(norm, normalize(float3(0.4, 1.0, 0.3))));
    float light = 0.55 * mix(0.35, 1.0, ao) + 0.45 * sun;
    return light;
}

fragment float4 _main(
    vs_in in [[stage_in]],
    constant uint8_t *voxel_dat [[buffer(8)]],
    constant uint *palette [[buffer(9)]]
) {
    int3 vox = int3(floor(in.local_pos - 0.5 * in.normal));
    vox = clamp(vox, int3(0), int3(31));
    uint8_t data = voxel_dat[(vox.x+1) + (vox.y+1) * 34 + (vox.z+1) * 34*34];
    float3 color = unpack_unorm4x8_to_float(palette[data]).rgb;
    float light = get_light(voxel_dat, in.local_pos, in.normal);
    return float4(color * light, 1.0);
}
