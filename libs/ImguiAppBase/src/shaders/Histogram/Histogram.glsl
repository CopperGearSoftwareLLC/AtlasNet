#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// Input image: normalized 0–1 range
layout (binding = 0, rgba8) uniform image2D uInput;

layout (std430, binding = 1) buffer Histogram {
    uint bins[256];
};

uniform ivec2 uSize;

void main()
{
   // bins[0] = bins[1];
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (gid.x >= uSize.x || gid.y >= uSize.y) return;

    // Read pixel (returns normalized floats)
    vec4 pixel = imageLoad(uInput, gid);

    // Compute luminance
    float y = dot(pixel.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Map to [0,255]
    uint bin = uint(round(clamp(y, 0.0, 1.0) * 255.0));

    atomicAdd(bins[bin], 1u);
}
