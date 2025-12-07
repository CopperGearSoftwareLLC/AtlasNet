#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// Input image (read-only normalized RGBA8)
layout (binding = 0, rgba8) uniform image2D uInput;
// Output image (write-only normalized RGBA8)
layout (binding = 1, rgba8) writeonly uniform image2D uOutput;

// LUT buffer
layout (std430, binding = 2) buffer Lut {
    uint lut[256];
};

uniform ivec2 uSize;

void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (gid.x >= uSize.x || gid.y >= uSize.y)
        return;

    // Read normalized [0–1] color
    vec4 pixel = imageLoad(uInput, gid);

    // Compute luminance in [0–1]
    float y = dot(pixel.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Use LUT index in [0,255]
    uint idx = uint(round(y * 255.0));
    uint eq = clamp(lut[idx], 0u, 255u);

    // Compute target luminance [0–1]
    float targetY = float(eq) / 255.0;

    // Scale RGB channels to preserve color ratios
    float scale = (y > 1e-6) ? (targetY / y) : 0.0;
    vec3 newRgb = clamp(pixel.rgb * scale, 0.0, 1.0);

    // Write result back to output (normalized)
    imageStore(uOutput, gid, vec4(newRgb, 1.0));
}
