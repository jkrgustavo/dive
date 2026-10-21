#ifndef UTIL_H
#define UTIL_H

#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <cimgui.h>
#include <sokol_imgui.h>
#include <assert.h>

#include <simd/simd.h>
#include <simd/math.h>
#include <simd/matrix.h>
#include <simd/matrix_types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef simd_float2 float2;
typedef simd_float3 float3;
typedef simd_float4 float4;

typedef simd_double2 double2;
typedef simd_double3 double3;
typedef simd_double4 double4;

typedef simd_char2 char2;
typedef simd_char3 char3;
typedef simd_char4 char4;

typedef simd_uchar2 uchar2;
typedef simd_uchar3 uchar3;
typedef simd_uchar4 uchar4;

typedef simd_int2 int2;
typedef simd_int3 int3;
typedef simd_int4 int4;

typedef simd_uint2 uint2;
typedef simd_uint3 uint3;
typedef simd_uint4 uint4;

typedef simd_long2 long2;
typedef simd_long3 long3;
typedef simd_long4 long4;

typedef simd_ulong2 ulong2;
typedef simd_ulong3 ulong3;
typedef simd_ulong4 ulong4;

// Reads a file's contents into a malloc'ed buffer. User must free buffer
char *read_file(const char* path);


static inline i32 rand_range(i32 low, i32 high) {
    return (rand() % (high - low + 1)) + low;
}

static inline f32 rand_norm() {
    return (float)rand() / (float)RAND_MAX;
}

static inline f32 rand_range_float(f32 low, f32 high) {
    return (rand_norm() * (high - low)) + low;
}

#endif /* UTIL_H */
