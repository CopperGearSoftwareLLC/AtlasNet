#version 450
layout(local_size_x = 16, local_size_y = 16) in;

// input images (read-only)
layout(binding = 0, rgba8) readonly uniform image2D imageA;
layout(binding = 1, rgba8) readonly uniform image2D imageB;

// output image (write-only)
layout(binding = 2, rgba8) writeonly uniform image2D outputImage;

// uniforms
uniform ivec2 ImageSize;
uniform int uOp; // 0=add, 1=subtract, 2=product

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(gid, ImageSize)))
        return;

    vec3 a = imageLoad(imageA, gid).xyz;
    vec3 b = imageLoad(imageB, gid).xyz;
    vec3 result;

    if (uOp == 0)
        result = a + b;
    else if (uOp == 1)
        result = a - b;
    else if (uOp == 2)
        result = a * b;
    else
        result = a;

    // clamp to [0,1] before writing back to prevent overflow
    result = clamp(result, 0.0, 1.0);
    imageStore(outputImage, gid, vec4(result,1.0));
}
