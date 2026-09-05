#version 410
// Main supplies inverse(world_to_clip), camera_position; depth writes off.
uniform mat4 clip_to_world;
uniform vec3 camera_position;
in vec2 clip_position;
out vec4 fColor;

// Keep this formula identical to voxel.frag: fog converges exactly to sky.
vec3 worldSky(vec3 direction) {
    vec3 sun = normalize(vec3(0.35, 0.80, 0.45));
    vec3 sky = mix(vec3(0.78, 0.84, 0.92), vec3(0.30, 0.50, 0.90),
                   pow(max(direction.y, 0.0), 0.6));
    sky = mix(sky, vec3(0.80, 0.82, 0.86), smoothstep(0.0, 0.6, -direction.y));
    float sun_dot = max(dot(direction, sun), 0.0);
    return sky + vec3(1.0, 0.95, 0.85) *
        (smoothstep(0.9985, 0.9995, sun_dot) * 8.0 + pow(sun_dot, 64.0) * 0.4);
}
vec3 reinhard_jodie(vec3 v) {
    float l = dot(v, vec3(0.2126, 0.7152, 0.0722));
    vec3 tv = v / (1.0 + v);
    return mix(v / (1.0 + l), tv, tv);
}
void main() {
    vec4 point = clip_to_world * vec4(clip_position, 1, 1);
    vec3 direction = normalize(point.xyz / point.w - camera_position);
    fColor = vec4(reinhard_jodie(worldSky(direction) * 0.9), 1);
}
