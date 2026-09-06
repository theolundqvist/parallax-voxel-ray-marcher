#version 410
// Positions are metres relative to the camera's anchor chunk corner; sea_level is in that frame.
uniform sampler2D color_buffer;
uniform sampler2D distance_buffer;
uniform mat4 clip_to_world;
uniform vec3 camera_position;
uniform float sea_level;
uniform vec3 sun_direction;
in vec2 clip_position;
out vec4 fColor;

const float FogDistance = 7300.0;
const float FogPower = 1.5;
const vec3 FogTint = vec3(0.92, 1.0, 1.10);
const float NoHit = 1e29;
const vec3 WaterSigma = vec3(0.35, 0.12, 0.08);
const vec3 WaterColor = vec3(0.02, 0.18, 0.24);
const float TotalReflectionCos = 0.66;

vec3 reinhard_jodie(vec3 v) {
    float l = dot(v, vec3(0.2126, 0.7152, 0.0722));
    vec3 tv = v / (1.0 + v);
    return mix(v / (1.0 + l), tv, tv);
}

// Same tone mapping as voxel.frag so fog converges exactly to sky.
vec3 sky(vec3 direction) {
    vec3 color = mix(vec3(0.78, 0.84, 0.92), vec3(0.30, 0.50, 0.90), pow(max(direction.y, 0.0), 0.6));
    color = mix(color, vec3(0.80, 0.82, 0.86), smoothstep(0.0, 0.6, -direction.y));
    float sun_dot = max(dot(direction, sun_direction), 0.0);
    color += vec3(1.0, 0.95, 0.85) * (smoothstep(0.9985, 0.9995, sun_dot) * 8.0 + pow(sun_dot, 64.0) * 0.4);
    return reinhard_jodie(color * 0.9);
}

// Per-channel extinction: blue attenuates faster, so the inscatter is sky-blue at mid range
// and converges exactly to the sky as every channel's transmittance reaches zero.
vec3 fogged(vec3 color, float distance, vec3 direction) {
    vec3 transmittance = exp(-pow(distance / FogDistance, FogPower) * FogTint);
    return mix(sky(direction), color, transmittance);
}

float fresnel(float cos_theta) {
    return 0.02 + 0.98 * pow(1.0 - cos_theta, 5.0);
}

float hash(vec2 cell) {
    return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(cell), hash(cell + vec2(1, 0)), f.x),
               mix(hash(cell + vec2(0, 1)), hash(cell + vec2(1, 1)), f.x), f.y);
}

float waves(vec2 p) {
    return noise(p * 0.35) * 0.7 + noise(p * 1.3 + 17.0) * 0.3;
}

vec3 waveNormal(vec2 p, float distance) {
    float e = 0.05;
    float amplitude = 0.15 / (1.0 + distance / 200.0);
    float dx = waves(p + vec2(e, 0)) - waves(p - vec2(e, 0));
    float dz = waves(p + vec2(0, e)) - waves(p - vec2(0, e));
    return normalize(vec3(-dx * amplitude / (2.0 * e), 1.0, -dz * amplitude / (2.0 * e)));
}

void main() {
    vec4 point = clip_to_world * vec4(clip_position, 1, 1);
    vec3 direction = normalize(point.xyz / point.w - camera_position);
    ivec2 texel = ivec2(gl_FragCoord.xy);
    vec3 color = texelFetch(color_buffer, texel, 0).rgb;
    float distance = texelFetch(distance_buffer, texel, 0).r;
    bool hit = distance < NoHit;
    float height = camera_position.y - sea_level;
    float sea_distance = direction.y != 0.0 ? -height / direction.y : -1.0;
    bool sea = sea_distance > 0.0 && (!hit || sea_distance < distance);
    vec3 result;
    if (height >= 0.0) {
        if (sea) {
            vec3 surface = camera_position + direction * sea_distance;
            vec3 normal = waveNormal(surface.xz, sea_distance);
            vec3 reflected = reflect(direction, normal);
            reflected.y = abs(reflected.y);
            vec3 transmit = exp(-WaterSigma * (hit ? distance - sea_distance : 1e6));
            vec3 below = mix(WaterColor, color, transmit);
            result = fogged(mix(below, sky(reflected), fresnel(-direction.y)), sea_distance, direction);
        } else {
            result = hit ? fogged(color, distance, direction) : sky(direction);
        }
    } else if (sea) {
        float reflectance = direction.y < TotalReflectionCos ? 1.0 : fresnel(direction.y);
        vec3 above = mix(sky(direction), WaterColor, reflectance);
        result = mix(WaterColor, above, exp(-WaterSigma * sea_distance));
    } else {
        result = mix(WaterColor, color, exp(-WaterSigma * (hit ? distance : 1e6)));
    }
    fColor = vec4(result, 1);
}
