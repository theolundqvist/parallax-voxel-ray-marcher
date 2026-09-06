#version 410
// Positions are metres relative to the camera's anchor chunk corner.
uniform sampler2D color_buffer;
uniform sampler2D distance_buffer;
uniform mat4 clip_to_world;
uniform vec3 camera_position;
uniform vec3 sun_direction;
in vec2 clip_position;
out vec4 fColor;

const float FogDistance = 7300.0;
const float FogPower = 1.5;
const vec3 FogTint = vec3(0.92, 1.0, 1.10);
const float NoHit = 1e29;
const vec3 WaterSigma = vec3(0.35, 0.12, 0.08);
const vec3 WaterColor = vec3(0.02, 0.18, 0.24);
// Critical angle for the water (n=1.333) to air interface.
const float TotalReflectionCos = 0.6612;

vec3 reinhard_jodie(vec3 v) {
    float l = dot(v, vec3(0.2126, 0.7152, 0.0722));
    vec3 tv = v / (1.0 + v);
    return mix(v / (1.0 + l), tv, tv);
}
vec3 sky(vec3 direction) {
    vec3 color = mix(vec3(0.78, 0.84, 0.92), vec3(0.30, 0.50, 0.90), pow(max(direction.y, 0.0), 0.6));
    color = mix(color, vec3(0.80, 0.82, 0.86), smoothstep(0.0, 0.6, -direction.y));
    float sun_dot = max(dot(direction, sun_direction), 0.0);
    color += vec3(1.0, 0.95, 0.85) * (smoothstep(0.9985, 0.9995, sun_dot) * 8.0 + pow(sun_dot, 64.0) * 0.4);
    return reinhard_jodie(color * 0.9);
}
vec3 fogged(vec3 color, float distance, vec3 direction) {
    vec3 transmittance = exp(-pow(distance / FogDistance, FogPower) * FogTint);
    return mix(sky(direction), color, transmittance);
}
float fresnel(float cos_theta) {
    return 0.02 + 0.98 * pow(1.0 - cos_theta, 5.0);
}
vec3 surfaceNormal(float code) {
    int face = int(abs(code)) - 1;
    vec3 normal = vec3(0);
    normal[face / 2] = (face % 2 == 0) ? 1.0 : -1.0;
    return normal;
}
void main() {
    vec4 point = clip_to_world * vec4(clip_position, 1, 1);
    vec3 direction = normalize(point.xyz / point.w - camera_position);
    ivec2 texel = ivec2(gl_FragCoord.xy);
    vec3 color = texelFetch(color_buffer, texel, 0).rgb;
    // Attachment contract shared with world.frag and the GPU benchmark readback.
    vec4 march = texelFetch(distance_buffer, texel, 0);
    bool hit = march.x < NoHit;
    bool surface = march.z < NoHit;
    bool underwater = march.w < 0.0;
    vec3 behind = hit ? fogged(color, max(march.x - march.y, 0.0), direction) : sky(direction);
    vec3 transmission = exp(-WaterSigma * march.y);
    vec3 result;
    if (underwater) {
        if (surface) {
            float cosine = clamp(dot(-direction, surfaceNormal(march.w)), 0.0, 1.0);
            float reflection = cosine < TotalReflectionCos ? 1.0 : fresnel(cosine);
            behind = mix(behind, WaterColor, reflection);
        }
        result = mix(WaterColor, behind, transmission);
    } else {
        result = mix(WaterColor, behind, transmission);
        if (surface) {
            vec3 normal = surfaceNormal(march.w);
            float reflection = fresnel(clamp(dot(-direction, normal), 0.0, 1.0));
            vec3 reflected = fogged(sky(reflect(direction, normal)), march.z, direction);
            result = mix(result, reflected, reflection);
        }
    }
    fColor = vec4(result, 1);
}
