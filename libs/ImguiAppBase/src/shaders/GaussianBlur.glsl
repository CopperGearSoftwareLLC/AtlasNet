#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// Input (read-only RGBA8)
layout (binding = 0, rgba8) readonly uniform image2D uInput;

// Output (write-only RGBA8)
layout (binding = 1, rgba8) writeonly uniform image2D uOutput;

// Uniforms
uniform ivec2 uSize;       // image dimensions
uniform int uKernelSize;   // kernel size N (odd number)
uniform float uSigma;      // standard deviation
uniform int uBoundaryMode; // 0 = CLAMP, 1 = MIRROR, 2 = ZERO_PAD

// Helper: mirror coordinate into range [0, size)
int MirrorCoord(int x, int size)
{
    if (size <= 1) return 0;
    int period = 2 * size - 2;
    x = abs(x);
    x = x % period;
    if (x >= size) x = period - x;
    return x;
}

// Read color with boundary handling
vec3 ReadRGB(ivec2 coord)
{
    if (uBoundaryMode == 0) // CLAMP
    {
        coord = clamp(coord, ivec2(0), uSize - ivec2(1));
        return imageLoad(uInput, coord).rgb;
    }
    else if (uBoundaryMode == 1) // MIRROR
    {
        coord.x = MirrorCoord(coord.x, uSize.x);
        coord.y = MirrorCoord(coord.y, uSize.y);
        return imageLoad(uInput, coord).rgb;
    }
    else if (uBoundaryMode == 2) // ZERO_PAD
    {
        if (coord.x < 0 || coord.y < 0 || coord.x >= uSize.x || coord.y >= uSize.y)
            return vec3(0.0);
        return imageLoad(uInput, coord).rgb;
    }

    // Default fallback: clamp
    coord = clamp(coord, ivec2(0), uSize - ivec2(1));
    return imageLoad(uInput, coord).rgb;
}

// Compute 2D Gaussian weight
float Gaussian(float x, float y, float sigma)
{
    float coeff = 1.0 / (2.0 * 3.14159265 * sigma * sigma);
    float exponent = -(x * x + y * y) / (2.0 * sigma * sigma);
    return coeff * exp(exponent);
}

void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (gid.x >= uSize.x || gid.y >= uSize.y)
        return;

    int halfs = uKernelSize / 2;
    float weightSum = 0.0;
    vec3 accum = vec3(0.0);

    // Apply Gaussian kernel dynamically
    for (int j = -halfs; j <= halfs; ++j)
    {
        for (int i = -halfs; i <= halfs; ++i)
        {
            float w = Gaussian(float(i), float(j), uSigma);
            vec3 color = ReadRGB(gid + ivec2(i, j));
            accum += color * w;
            weightSum += w;
        }
    }

    // Normalize
    vec3 result = accum / weightSum;
    imageStore(uOutput, gid, vec4(result, 1.0));
}
