#include <metal_stdlib>

using namespace metal;

struct vs_out {
    float4 position [[position]];
    float4 color;
};

struct vs_in {
    float4 pos [[attribute(0)]];
    float4 col [[attribute(1)]];
};

vertex vs_out _main(vs_in in [[stage_in]]) {
    vs_out out;
    out.position = in.pos;
    out.color = in.col;
    return out;
}
