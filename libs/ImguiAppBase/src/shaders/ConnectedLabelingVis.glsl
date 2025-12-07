#version 450
layout(local_size_x = 16, local_size_y = 16) in;

// Input: unsigned 32-bit single-channel texture
layout(r32ui,  binding = 0) readonly  uniform uimage2D InValues;
// Output: normalized RGBA (e.g., GL_RGBA8)
layout(rgba8,  binding = 1) writeonly uniform image2D  OutColors;

uniform ivec2 ImageSize;
uniform bool  uZeroTransparent; // true -> value 0 gets alpha 0

// SplitMix32-style hash: fast, good bit diffusion
uint hash32(uint x) {
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    x =  x ^ (x >> 16);
    return x;
}

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(gid, ImageSize))) return;

    uint v = imageLoad(InValues, gid).r;

    if (uZeroTransparent && v == 0u) {
        imageStore(OutColors, gid, vec4(0.0, 0.0, 0.0, 0.0));
        return;
    }

    uint h = hash32(v);               // 32-bit hash of the label/value
    uvec3 rgb8 = uvec3( h        & 255u,
                       (h >> 8)  & 255u,
                       (h >> 16) & 255u);  // take 24 bits as RGB

    vec4 color = vec4(vec3(rgb8) / 255.0, 1.0);
    imageStore(OutColors, gid, color);
}
