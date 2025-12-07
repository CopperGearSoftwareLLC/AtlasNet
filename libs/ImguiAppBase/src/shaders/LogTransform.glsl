#version 460
#extension GL_ARB_compute_shader : enable
#extension GL_ARB_shader_image_load_store : enable

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba8) readonly uniform image2D inputImage;
layout (binding = 1, rgba8) writeonly uniform image2D outputImage;

uniform float uC;
void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(inputImage);

    // Skip out-of-bounds threads
    if (coord.x >= size.x || coord.y >= size.y)
        return;

    vec4 color = imageLoad(inputImage, coord);

    vec3 transformed = uC * log(1.0 + color.rgb);

    transformed = clamp(transformed, 0.0, 1.0);

    imageStore(outputImage, coord, vec4(transformed, color.a));
}
