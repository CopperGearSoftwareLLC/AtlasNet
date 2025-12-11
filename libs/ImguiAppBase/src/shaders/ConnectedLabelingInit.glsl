#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(r8,  binding = 0) readonly  uniform image2D inputImage;
layout(r32ui, binding = 1) writeonly uniform uimage2D labelsOut;

uniform ivec2 ImageSize;
uniform int uConnMode;
uniform float Threshold;

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(gid, ImageSize))) return;

    float on = float(imageLoad(inputImage, gid).r);
    uint id = (on >Threshold)? uint(gid.y) * uint(ImageSize.x) + uint(gid.x) + 1u : 0u;
    imageStore(labelsOut, gid, uvec4(id));
}
