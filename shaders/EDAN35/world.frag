#version 410
// Merge non-overlapping drawn chunks from all LODs in ray-distance order. Partial streaming
// may interleave levels; marching each entire level in turn is not a valid transparency order.
uniform usampler3D volume;
uniform usampler3D coarse_occupancy;
uniform usampler3D page_tables;
uniform bool world_acceleration = true;
uniform mat4 clip_to_world;
uniform vec3 camera_position;
uniform vec3 sun_direction;
uniform vec3 colorPalette[256];

const int PageSize = 9;
const int LevelCount = 10;
const int OccupancyCells = 4;
const int Water = 9;
const float NoHit = 1e30;
struct level_t {
    vec4 origin_span;
    ivec4 size_drawn;
    ivec4 page_origin_top;
    ivec4 hole_lo;
    ivec4 hole_hi;
};
layout (std140) uniform Levels { level_t levels[LevelCount]; };
in vec2 clip_position;
layout (location = 0) out vec4 fColor;
// Opaque metres, accumulated water metres, first air/water boundary metres, signed face code.
// Faces: +X,-X,+Y,-Y,+Z,-Z = 1..6, oriented against the ray. Negative code means
// camera underwater (first boundary is an exit); -7 means underwater with no visible boundary.
layout (location = 1) out vec4 fMarch;
struct march_t {
    float distance;
    vec3 normal;
    int material;
    float water_length;
    float water_start;
    float surface;
    float face;
    bool in_water;
};
struct cursor_t {
    float t;
    float exit;
    float next;
    ivec3 chunk;
    vec3 normal;
    uint entry;
};

float nearest(vec3 times) { return min(times.x, min(times.y, times.z)); }
vec3 boundaryTimes(vec3 origin, vec3 direction, ivec3 chunk, ivec3 low, ivec3 high) {
    vec3 times = vec3(NoHit);
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] != 0.0) {
            int boundary = direction[axis] > 0.0 ? high[axis] : low[axis];
            times[axis] = (float(chunk[axis]) + float(boundary) / 32.0 - origin[axis]) / direction[axis];
        }
    }
    return times;
}
vec3 crossingNormal(bvec3 crossed, ivec3 step_dir) {
    if (crossed.x) return vec3(-step_dir.x, 0, 0);
    if (crossed.y) return vec3(0, -step_dir.y, 0);
    return vec3(0, 0, -step_dir.z);
}
float faceCode(vec3 normal) {
    if (normal.x != 0.0) return normal.x > 0.0 ? 1.0 : 2.0;
    if (normal.y != 0.0) return normal.y > 0.0 ? 3.0 : 4.0;
    return normal.z > 0.0 ? 5.0 : 6.0;
}
// Integrate whole contiguous runs, not individual steps: occupancy skipping then uses exactly
// the same endpoint subtraction as the reference walk, including arbitrarily many air pockets.
void medium(inout march_t m, bool water, float t, vec3 normal) {
    if (water == m.in_water) return;
    if (m.in_water) m.water_length += t - m.water_start;
    else m.water_start = t;
    if (t == 0.0 && water) m.face = -7.0;
    else if (m.surface >= NoHit) {
        m.surface = t;
        m.face = faceCode(normal) * (water ? 1.0 : -1.0);
    }
    m.in_water = water;
}
void solid(inout march_t m, int material, float t, vec3 normal) {
    if (m.in_water) m.water_length += t - m.water_start;
    m.in_water = false;
    m.distance = t;
    m.normal = normal;
    m.material = material;
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
void brickHit(vec3 ro, vec3 rd, ivec3 step_dir, cursor_t c, ivec3 brick, float span, inout march_t m) {
    ivec3 cell = voxelAt(ro, rd, c.chunk, c.t, ivec3(0), ivec3(32)) / 8;
    float cell_entry = c.t;
    vec3 normal = c.normal;
    for (int coarse_step = 0; coarse_step < 12; ++coarse_step) {
        ivec3 low = cell * 8;
        ivec3 high = low + 8;
        vec3 cell_times = boundaryTimes(ro, rd, c.chunk, low, high);
        float cell_exit = nearest(cell_times);
        // Presence bits: opaque=1, water=2, air=4. Only homogeneous transparent cells
        // may bypass fine traversal; mixed water/air must preserve every transition.
        int occupancy = world_acceleration ? int(texelFetch(coarse_occupancy, brick * 4 + cell, 0).r) : 7;
        if (occupancy == 2 || occupancy == 4) {
            medium(m, occupancy == 2, cell_entry * span, normal);
        } else {
            ivec3 voxel = voxelAt(ro, rd, c.chunk, cell_entry, low, high);
            float t = cell_entry;
            vec3 face_normal = normal;
            for (int fine_step = 0; fine_step < 24; ++fine_step) {
                int material = int(texelFetch(volume, brick * 32 + voxel, 0).r);
                bool water = (material & 15) == Water;
                if (material != 0 && !water) {
                    solid(m, material, t * span, face_normal);
                    return;
                }
                medium(m, water, t * span, face_normal);
                vec3 times = boundaryTimes(ro, rd, c.chunk, voxel, voxel + 1);
                float next_t = nearest(times);
                if (next_t >= cell_exit) break;
                bvec3 crossed = lessThanEqual(times, vec3(next_t));
                voxel += ivec3(crossed) * step_dir;
                face_normal = crossingNormal(crossed, step_dir);
                t = next_t;
            }
        }
        if (cell_exit >= c.next) return;
        bvec3 crossed = lessThanEqual(cell_times, vec3(cell_exit));
        cell += ivec3(crossed) * step_dir;
        normal = crossingNormal(crossed, step_dir);
        cell_entry = cell_exit;
    }
}
void advanceCursor(vec3 ro, vec3 rd, ivec3 step_dir, inout cursor_t c) {
    vec3 times = boundaryTimes(ro, rd, c.chunk, ivec3(0), ivec3(32));
    c.t = nearest(times);
    bvec3 crossed = lessThanEqual(times, vec3(c.t));
    c.chunk += ivec3(crossed) * step_dir;
    c.normal = crossingNormal(crossed, step_dir);
}
void seekChunk(int level, vec3 ro, vec3 rd, ivec3 step_dir, inout cursor_t c) {
    level_t L = levels[level];
    c.entry = 0u;
    for (int step = 0; step < 3 * PageSize; ++step) {
        if (c.t >= c.exit || any(lessThan(c.chunk, ivec3(0))) || any(greaterThanEqual(c.chunk, L.size_drawn.xyz))) return;
        if (all(greaterThanEqual(c.chunk, L.hole_lo.xyz)) && all(lessThan(c.chunk, L.hole_hi.xyz))) {
            vec3 times = boundaryTimes(ro, rd, ivec3(0), L.hole_lo.xyz * 32, L.hole_hi.xyz * 32);
            c.t = nearest(times);
            c.normal = crossingNormal(lessThanEqual(times, vec3(c.t)), step_dir);
            if (c.t >= c.exit) return;
            c.chunk = voxelAt(ro, rd, ivec3(0), c.t, ivec3(0), L.size_drawn.xyz * 32) / 32;
        }
        ivec3 texel = (L.page_origin_top.xyz + c.chunk) % PageSize;
        c.entry = texelFetch(page_tables, texel + ivec3(0, 0, level * PageSize), 0).r;
        if (c.entry != 0u) {
            c.next = nearest(boundaryTimes(ro, rd, c.chunk, ivec3(0), ivec3(32)));
            return;
        }
        advanceCursor(ro, rd, step_dir, c);
    }
}
cursor_t beginCursor(int level, vec3 ro, vec3 rd, ivec3 step_dir) {
    cursor_t c = cursor_t(0.0, NoHit, 0.0, ivec3(0), -rd, 0u);
    level_t L = levels[level];
    if (L.size_drawn.w == 0) return c;
    vec3 extent = vec3(L.size_drawn.x, L.page_origin_top.w + 1, L.size_drawn.z);
    for (int axis = 0; axis < 3; ++axis) {
        if (rd[axis] == 0.0) {
            if (ro[axis] < 0.0 || ro[axis] >= extent[axis]) return c;
        } else {
            float a = -ro[axis] / rd[axis];
            float b = (extent[axis] - ro[axis]) / rd[axis];
            float near_t = min(a, b);
            if (near_t > c.t) {
                c.t = near_t;
                c.normal = vec3(0);
                c.normal[axis] = -float(step_dir[axis]);
            }
            c.exit = min(c.exit, max(a, b));
        }
    }
    if (c.t >= c.exit) return c;
    c.chunk = voxelAt(ro, rd, ivec3(0), c.t, ivec3(0), L.size_drawn.xyz * 32) / 32;
    seekChunk(level, ro, rd, step_dir, c);
    return c;
}
vec3 reinhard_jodie(vec3 v) {
    float l = dot(v, vec3(0.2126, 0.7152, 0.0722));
    vec3 tv = v / (1.0 + v);
    return mix(v / (1.0 + l), tv, tv);
}
vec3 worldShade(march_t hit) {
    vec3 albedo = colorPalette[hit.material];
    vec3 ambient = mix(vec3(0.28, 0.24, 0.20), vec3(0.55, 0.68, 0.85), hit.normal.y * 0.5 + 0.5);
    vec3 color = albedo * (ambient + vec3(1.0, 0.95, 0.85) * 2.2 * max(dot(hit.normal, sun_direction), 0.0));
    if ((hit.material & 15) == 5) color = albedo * 1.9;
    if (hit.distance == 0.0) color = vec3(0.05, 0.05, 0.06);
    return reinhard_jodie(color * 0.9);
}
void main() {
    vec4 point = clip_to_world * vec4(clip_position, 1, 1);
    vec3 rd = normalize(point.xyz / point.w - camera_position);
    ivec3 step_dir = ivec3(sign(rd));
    cursor_t cursors[LevelCount];
    for (int level = 0; level < LevelCount; ++level) {
        vec3 ro = (camera_position - levels[level].origin_span.xyz) / levels[level].origin_span.w;
        cursors[level] = beginCursor(level, ro, rd, step_dir);
    }
    march_t m = march_t(NoHit, vec3(0), 0, 0.0, 0.0, NoHit, 0.0, false);
    float previous_exit = 0.0;
    vec3 exit_normal = -rd;
    for (int step = 0; step < LevelCount * 3 * PageSize; ++step) {
        int selected = -1;
        float t = NoHit;
        for (int level = 0; level < LevelCount; ++level) {
            float candidate = cursors[level].t * levels[level].origin_span.w;
            if (cursors[level].entry != 0u && candidate < t) {
                selected = level;
                t = candidate;
            }
        }
        if (selected < 0) break;
        if (t > previous_exit) medium(m, false, previous_exit, exit_normal);
        cursor_t c = cursors[selected];
        float span = levels[selected].origin_span.w;
        vec3 ro = (camera_position - levels[selected].origin_span.xyz) / span;
        if (c.entry >= 256u) brickHit(ro, rd, step_dir, c, brickOf(c.entry), span, m);
        else if ((c.entry & 15u) == uint(Water)) medium(m, true, t, c.normal);
        else solid(m, int(c.entry), t, c.normal);
        if (m.distance < NoHit) break;
        previous_exit = c.next * span;
        advanceCursor(ro, rd, step_dir, c);
        exit_normal = c.normal;
        seekChunk(selected, ro, rd, step_dir, c);
        cursors[selected] = c;
    }
    // Beyond the recorded frontier is unknown/air, not an infinite analytical ocean.
    if (m.in_water) medium(m, false, previous_exit, exit_normal);
    fMarch = vec4(m.distance, m.water_length, m.surface, m.face);
    fColor = m.distance >= NoHit ? vec4(0, 0, 0, 1) : vec4(worldShade(m), 1);
}
