#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// Input (read-only RGBA8)
layout (binding = 0, rgba8) readonly uniform image2D uInput;

// Output (write-only RGBA8)
layout (binding = 1, rgba8) writeonly uniform image2D uOutput;

// Uniforms
uniform ivec2 uSize;   // image dimensions
uniform float uScale;  // edge intensity scaling factor

// Safe grayscale sampler from image2D
float ReadGray(ivec2 coord)
{
    coord = clamp(coord, ivec2(0), uSize - ivec2(1));
    vec4 color = imageLoad(uInput, coord);
    return dot(color.rgb, vec3(0.299, 0.587, 0.114));
}

void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (gid.x >= uSize.x || gid.y >= uSize.y)
        return;

    // Offsets for neighbor sampling
    ivec2 t = ivec2(1, 1);

    // 3×3 neighborhood grayscale
    float tl = ReadGray(gid + ivec2(-t.x, -t.y));
    float  l = ReadGray(gid + ivec2(-t.x,  0));
    float bl = ReadGray(gid + ivec2(-t.x,  t.y));

    float tr = ReadGray(gid + ivec2( t.x, -t.y));
    float  r = ReadGray(gid + ivec2( t.x,  0));
    float br = ReadGray(gid + ivec2( t.x,  t.y));

    float  tC = ReadGray(gid + ivec2(0, -t.y));
    float  bC = ReadGray(gid + ivec2(0,  t.y));

    // Sobel filter
    float gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    float gy = -tl - 2.0*tC - tr + bl + 2.0*bC + br;

    // Edge magnitude scaled
    float edge = clamp(length(vec2(gx, gy)) * uScale, 0.0, 1.0);

    // Write result (grayscale in RGB)
    imageStore(uOutput, gid, vec4(vec3(edge), 1.0));
}
