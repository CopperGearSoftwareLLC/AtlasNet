#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(r8,  binding = 0) readonly  uniform image2D inputImage;
layout(r32ui, binding = 1) readonly  uniform uimage2D labelsIn;
layout(r32ui, binding = 2) writeonly uniform uimage2D labelsOut;

// changedFlag[0] is set to 1 if any pixel label changes in this Dispatch16_16_1
layout(std430, binding = 3) buffer ChangeFlag { uint changedFlag; };

uniform ivec2 ImageSize;
uniform int uConnMode;// 0=4, 1=8, 2=m
uniform float Threshold;


// --- adjacency helpers (same logic as above, adapted to labels) ---
bool inBounds(ivec2 p) {
    return all(greaterThanEqual(p, ivec2(0))) &&
           all(lessThan(p, ImageSize));
}

bool isOn(ivec2 p) {
    if (!inBounds(p)) return false;
    return (float(imageLoad(inputImage, p).r)) > Threshold;
}

bool m_allows_diagonal(ivec2 p, ivec2 q) {
    // p and q are diagonal 8-neighbors; allow only if both shared 4-neighbors are 0
    ivec2 d = q - p;
    ivec2 a = ivec2(p.x + d.x, p.y);
    ivec2 b = ivec2(p.x,       p.y + d.y);
    return !(isOn(a) || isOn(b));
}

// Return whether q is an allowed neighbor of p under the selected topology
bool neighbor_allowed(ivec2 p, ivec2 q) {
    ivec2 d = q - p;
    if (max(abs(d.x), abs(d.y)) != 1) return false; // not 8-neighbor
    int man = abs(d.x) + abs(d.y);                  // 1=4-neigh, 2=diagonal

    if (uConnMode == 0) { // 4-connected
        return man == 1;
    } else if (uConnMode == 1) { // 8-connected
        return true;
    } else { // m-connected
        if (man == 1) return true; // orthogonal ok
        // diagonal: only if mixed rule allows
        return m_allows_diagonal(p, q);
    }
}

// Iterate once: take the minimum nonzero neighbor label (respecting topology).
void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if (!inBounds(gid)) return;

    float on = float(imageLoad(inputImage, gid).r);
    uint myLabel = imageLoad(labelsIn, gid).r;

    if (on <= Threshold) {
        imageStore(labelsOut, gid, uvec4(0u));
        return;
    }

    uint best = (myLabel == 0u) ? myLabel : myLabel;

    // Scan neighbors
    for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        ivec2 q = gid + ivec2(dx, dy);
        if (!inBounds(q)) continue;
        if (!isOn(q))     continue;
        if (!neighbor_allowed(gid, q)) continue;

        uint qLabel = imageLoad(labelsIn, q).r;
        if (qLabel != 0u && (best == 0u || qLabel < best))
            best = qLabel;
    }

    // If we found a smaller label, update and mark change
    if (best != 0u && best < myLabel) {
        imageStore(labelsOut, gid, uvec4(best));
        atomicExchange(changedFlag, 1u);
    } else {
        imageStore(labelsOut, gid, uvec4(myLabel));
    }
}
