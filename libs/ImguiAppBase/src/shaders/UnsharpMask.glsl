#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// Input/output
layout (binding = 0, rgba8) readonly uniform image2D uInput;
layout (binding = 1, rgba8) writeonly uniform image2D uOutput;

// Parameters
uniform ivec2 uSize;
uniform float uSigma;
uniform int uKernelSize;
uniform float uStrength;
uniform int uBoundaryMode; // 0 = CLAMP, 1 = MIRROR, 2 = ZERO_PAD

// Mirror coordinate into [0, size)
int MirrorCoord(int x, int size)
{
    if (size <= 1) return 0;
    int period = 2 * size - 2;
    x = abs(x);
    x = x % period;
    if (x >= size) x = period - x;
    return x;
}

// Safe texture fetch with boundary handling
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

// Gaussian 1D kernel
float gaussian(float x, float sigma)
{
    return exp(-0.5 * (x * x) / (sigma * sigma)) / (2.0 * 3.14159265 * sigma * sigma);
}

void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (gid.x >= uSize.x || gid.y >= uSize.y)
        return;

    int halfK = uKernelSize / 2;

    // Compute Gaussian weights (1D separable approximation)
    float weights[64]; // supports up to 63x63 kernel
    float sum = 0.0;
    for (int i = 0; i < uKernelSize; ++i)
    {
        float x = float(i - halfK);
        weights[i] = gaussian(x, uSigma);
        sum += weights[i];
    }
    for (int i = 0; i < uKernelSize; ++i)
        weights[i] /= sum;

    // Apply 2D Gaussian blur (separable style)
    vec3 blur = vec3(0.0);
    for (int j = -halfK; j <= halfK; ++j)
    {
        for (int i = -halfK; i <= halfK; ++i)
        {
            vec3 color = ReadRGB(gid + ivec2(i, j));
            float w = weights[i + halfK] * weights[j + halfK];
            blur += color * w;
        }
    }

    vec3 orig = ReadRGB(gid);
    vec3 sharp = orig + uStrength * (orig - blur);
    imageStore(uOutput, gid, vec4(clamp(sharp, 0.0, 1.0), 1.0));
}
