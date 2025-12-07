#version 430
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// Histogram in
layout (std430, binding = 0) buffer Histogram {
    uint bins[256];
};

// LUT out (0..255)
layout (std430, binding = 1) buffer Lut {
    uint lut[256];
};

// Total pixels (width * height)
uniform uint uTotalPixels;

// Shared memory for scan
shared uint s[256];

void main() {
    uint i = gl_LocalInvocationID.x;
    s[i] = bins[i];
    memoryBarrierShared();
    barrier();

    // Inclusive scan (Blelloch variant could be used; this is simple Hillis–Steele)
    for (uint offset = 1u; offset < 256u; offset <<= 1u) {
        uint v = 0u;
        if (i >= offset) v = s[i - offset];
        barrier();
        s[i] += v;
        barrier();
    }

    // Find cdf_min (first non-zero)
uint cdf_min = 0u;
if (i == 0u) {
    for (uint k = 0u; k < 256u; ++k) {
        if (s[k] > 0u) { cdf_min = s[k]; break; }
    }
}

// Find cdf_max (last non-zero)
uint cdf_max = 0u;
if (i == 0u) {
    for (int k = 255; k >= 0; --k) {
        if (s[k] > 0u) { cdf_max = s[k]; break; }
    }
}

// Broadcast both
barrier();
if (i == 0u) { s[0] = cdf_min; s[1] = cdf_max; }
barrier();
cdf_min = s[0];
cdf_max = s[1];

// Normalize to 0..255 using active histogram range
uint cdf_i = s[i];
uint denom = max(cdf_max - cdf_min, 1u);
float norm = clamp((float(cdf_i) - float(cdf_min)) / float(denom), 0.0, 1.0);
lut[i] = uint(norm * 255.0 + 0.5);
}
