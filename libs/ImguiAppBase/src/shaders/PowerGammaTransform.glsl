#version 460
#extension GL_ARB_compute_shader : enable
#extension GL_ARB_shader_image_load_store : enable


layout (local_size_x = 16, local_size_y = 16) in;

// Input/output images
layout (binding = 0, rgba8) readonly uniform image2D inputImage;
layout (binding = 1, rgba8) writeonly uniform image2D outputImage;

// Uniform parameters
layout (location = 0) uniform float uGamma;


vec3 applyGamma(vec3 color, float gamma)
{
    return pow(color, vec3(gamma));
}
void main()
{
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = imageLoad(inputImage, coords);
    
    color.rgb = applyGamma(color.rgb, uGamma);
    imageStore(outputImage, coords, color);
}
