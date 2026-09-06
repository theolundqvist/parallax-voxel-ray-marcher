#version 410
// One full-screen pass marches every recorded level from the camera outward. Each level is
// walked in its own chunk units exactly as a per-level pass would, restricted to distances
// nearer than the best hit so far, so the result is the nearest hit over all levels.
uniform sampler3D volume;
uniform sampler3D coarse_occupancy;
uniform usampler3D page_tables;
uniform bool world_acceleration = true;
uniform mat4 clip_to_world;
uniform vec3 camera_position;
uniform vec3 sun_direction;
uniform vec3 colorPalette[256];

const int PageSize = 9;
const int LevelCount = 10;
const int OccupancyCells = 4;
const float NoHit = 1e30;

struct level_t {
    vec4 origin_span;
    ivec4 size_drawn;
    ivec4 page_origin;
    ivec4 hole_lo;
    ivec4 hole_hi;
};
layout (std140) uniform Levels {
    level_t levels[LevelCount];
};

in vec2 clip_position;

layout (location = 0) out vec4 fColor;
layout (location = 1) out float fDistance;

struct hit_t {
    float distance;
    vec3 normal;
    int material;
};

vec3 reinhard_jodie(vec3 v) {
    float l = dot(v, vec3(0.2126, 0.7152, 0.0722));
    vec3 tv = v / (1.0 + v);
    return mix(v / (1.0 + l), tv, tv);
}

// Boundaries are voxel indices within a chunk (0..32) so chunk, cell and voxel faces agree exactly.
vec3 boundaryTimes(vec3 origin, vec3 direction, ivec3 chunk, ivec3 low, ivec3 high) {
    vec3 times = vec3(1e30);
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] != 0.0) {
            int boundary = direction[axis] > 0.0 ? high[axis] : low[axis];
            times[axis] = (float(chunk[axis]) + float(boundary) / 32.0 - origin[axis]) / direction[axis];
        }
    }
    return times;
}

float nearest(vec3 times) { return min(times.x, min(times.y, times.z)); }

vec3 crossingNormal(bvec3 crossed, ivec3 step_dir) {
    if (crossed.x) return vec3(-step_dir.x, 0, 0);
    if (crossed.y) return vec3(0, -step_dir.y, 0);
    return vec3(0, 0, -step_dir.z);
}

ivec3 brickOf(uint entry) {
    int slot = int(entry) - 256;
    ivec3 bricks = textureSize(coarse_occupancy, 0) / OccupancyCells;
    return ivec3(slot % bricks.x, (slot / bricks.x) % bricks.y, slot / (bricks.x * bricks.y));
}

ivec3 voxelAt(vec3 ro, vec3 rd, ivec3 chunk, float t, ivec3 low, ivec3 high) {
    vec3 p = (ro + rd * t - vec3(chunk)) * 32.0;
    ivec3 voxel = ivec3(floor(p));
    for (int axis = 0; axis < 3; ++axis)
        if (rd[axis] < 0.0 && p[axis] == floor(p[axis])) --voxel[axis];
    return clamp(voxel, low, high - 1);
}

// The reference and accelerated paths share the same coarse traversal and fine-cell entry
// arithmetic. Acceleration only omits an entirely empty cell; no approximate LOD, epsilon
// advance, or repeated t accumulation is involved.
bool brickHit(vec3 ro, vec3 rd, ivec3 step_dir, ivec3 chunk, ivec3 brick, float entry, float exit,
              vec3 normal, out float brick_t, out vec3 brick_normal, out int brick_material) {
    ivec3 cell = voxelAt(ro, rd, chunk, entry, ivec3(0), ivec3(32)) / 8;
    float cell_entry = entry;
    for (int coarse_step = 0; coarse_step < 12; ++coarse_step) {
        ivec3 low = cell * 8;
        ivec3 high = low + 8;
        vec3 cell_times = boundaryTimes(ro, rd, chunk, low, high);
        float cell_exit = nearest(cell_times);
        if (!world_acceleration || texelFetch(coarse_occupancy, brick * 4 + cell, 0).r != 0.0) {
            ivec3 voxel = voxelAt(ro, rd, chunk, cell_entry, low, high);
            float t = cell_entry;
            vec3 face_normal = normal;
            for (int fine_step = 0; fine_step < 24; ++fine_step) {
                int material = int(round(texelFetch(volume, brick * 32 + voxel, 0).r * 255.0));
                if (material != 0) {
                    brick_t = t;
                    brick_normal = face_normal;
                    brick_material = material;
                    return true;
                }
                vec3 times = boundaryTimes(ro, rd, chunk, voxel, voxel + 1);
                float next_t = nearest(times);
                if (next_t >= cell_exit) break;
                bvec3 crossed = lessThanEqual(times, vec3(next_t));
                voxel += ivec3(crossed) * step_dir;
                if (any(lessThan(voxel, low)) || any(greaterThanEqual(voxel, high))) break;
                face_normal = crossingNormal(crossed, step_dir);
                t = next_t;
            }
        }
        if (cell_exit >= exit) break;
        bvec3 crossed = lessThanEqual(cell_times, vec3(cell_exit));
        cell += ivec3(crossed) * step_dir;
        if (any(lessThan(cell, ivec3(0))) || any(greaterThanEqual(cell, ivec3(4)))) break;
        normal = crossingNormal(crossed, step_dir);
        cell_entry = cell_exit;
    }
    return false;
}

// Walks one level in its chunk units; only hits nearer than best.distance replace it.
void levelHit(int level, vec3 rd, inout hit_t best) {
    level_t L = levels[level];
    float span = L.origin_span.w;
    ivec3 region_size = L.size_drawn.xyz;
    ivec3 hole_lo = L.hole_lo.xyz;
    ivec3 hole_hi = L.hole_hi.xyz;
    vec3 ro = (camera_position - L.origin_span.xyz) / span;
    ivec3 step_dir = ivec3(sign(rd));
    float exit = nearest(boundaryTimes(ro, rd, ivec3(0), ivec3(0), region_size * 32));
    float t = 0.0;
    vec3 normal = -rd;
    bool hole = all(lessThan(hole_lo, hole_hi));
    if (hole && all(greaterThanEqual(ro, vec3(hole_lo))) && all(lessThan(ro, vec3(hole_hi)))) {
        vec3 hole_times = boundaryTimes(ro, rd, ivec3(0), hole_lo * 32, hole_hi * 32);
        t = nearest(hole_times);
        normal = crossingNormal(lessThanEqual(hole_times, vec3(t)), step_dir);
    }
    if (t >= exit || t * span >= best.distance) return;
    ivec3 chunk = voxelAt(ro, rd, ivec3(0), t, ivec3(0), region_size * 32) / 32;
    ivec3 texel = (L.page_origin.xyz + chunk) % PageSize;
    int slab = level * PageSize;
    for (int step = 0; step < 3 * PageSize; ++step) {
        vec3 times = boundaryTimes(ro, rd, chunk, ivec3(0), ivec3(32));
        float next = nearest(times);
        if (!(hole && all(greaterThanEqual(chunk, hole_lo)) && all(lessThan(chunk, hole_hi)))) {
            uint entry = texelFetch(page_tables, texel + ivec3(0, 0, slab), 0).r;
            if (entry >= 256u) {
                float brick_t;
                vec3 brick_normal;
                int brick_material;
                if (brickHit(ro, rd, step_dir, chunk, brickOf(entry), t, next, normal, brick_t, brick_normal, brick_material)) {
                    if (brick_t * span < best.distance) best = hit_t(brick_t * span, brick_normal, brick_material);
                    return;
                }
            } else if (entry != 0u) {
                if (t * span < best.distance) best = hit_t(t * span, normal, int(entry));
                return;
            }
        }
        if (next >= exit || next * span >= best.distance) return;
        ivec3 advance = ivec3(lessThanEqual(times, vec3(next))) * step_dir;
        chunk += advance;
        if (any(lessThan(chunk, ivec3(0))) || any(greaterThanEqual(chunk, region_size))) return;
        texel += advance;
        texel += ivec3(lessThan(texel, ivec3(0))) * PageSize - ivec3(greaterThanEqual(texel, ivec3(PageSize))) * PageSize;
        normal = crossingNormal(bvec3(advance), step_dir);
        t = next;
    }
}

// Fog and sky are applied by composite.frag from the distance attachment.
vec3 worldShade(hit_t hit) {
    vec3 albedo = colorPalette[hit.material];
    vec3 ambient = mix(vec3(0.28, 0.24, 0.20), vec3(0.55, 0.68, 0.85), hit.normal.y * 0.5 + 0.5);
    vec3 color = albedo * (ambient + vec3(1.0, 0.95, 0.85) * 2.2 * max(dot(hit.normal, sun_direction), 0.0));
    if ((hit.material & 15) == 5) color = albedo * 1.9;
    if (hit.distance == 0.0) color = vec3(0.05, 0.05, 0.06);
    return reinhard_jodie(color * 0.9);
}

void main()
{
    vec4 point = clip_to_world * vec4(clip_position, 1, 1);
    vec3 rd = normalize(point.xyz / point.w - camera_position);
    hit_t best = hit_t(NoHit, vec3(0), 0);
    for (int level = 0; level < LevelCount; ++level)
        if (levels[level].size_drawn.w != 0) levelHit(level, rd, best);
    if (best.distance >= NoHit) {
        fColor = vec4(0, 0, 0, 1);
        fDistance = NoHit;
        return;
    }
    fColor = vec4(worldShade(best), 1);
    fDistance = best.distance;
}
