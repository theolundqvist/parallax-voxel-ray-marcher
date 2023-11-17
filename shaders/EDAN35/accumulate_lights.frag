#version 410

struct ViewProjTransforms
{
	mat4 view_projection;
	mat4 view_projection_inverse;
};

layout (std140) uniform CameraViewProjTransforms
{
	ViewProjTransforms camera;
};

layout (std140) uniform LightViewProjTransforms
{
	ViewProjTransforms lights[4];
};

uniform int light_index;

uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2D shadow_texture;

uniform vec2 inverse_screen_resolution;

uniform vec3 camera_position;

uniform vec3 light_color;
uniform vec3 light_position;
uniform vec3 light_direction;
uniform float light_intensity;
uniform float light_angle_falloff;

layout (location = 0) out vec4 light_diffuse_contribution;
layout (location = 1) out vec4 light_specular_contribution;

const float PI = 3.1415926535897932384626433832795;
const float PI_2 = 1.57079632679489661923;
const float PI_4 = 0.785398163397448309616;

void main()
{

	vec2 shadowmap_texel_size = 1.0f / textureSize(shadow_texture, 0);

  vec2 uv = gl_FragCoord.xy * inverse_screen_resolution;
  float depth = texture(depth_texture, uv).r;
  vec4 NDC = vec4(uv, depth, 1.0f) * 2.0f - 1.0f;
  vec4 world_orth = camera.view_projection_inverse * NDC;
  vec3 world = world_orth.xyz / world_orth.w;

  vec3 N = normalize(texture(normal_texture, uv).xyz * 2.0f - 1.0f);
  

  vec4 light_view = lights[light_index].view_projection * vec4(world, 1.0f);
  vec3 light_proj = light_view.xyz / light_view.w;
  vec3 light_NDC = light_proj * 0.5f + 0.5f;
  float shadow_depth = texture(shadow_texture, light_NDC.xy).r;

  float distance = length(world - light_position);

  //if(shadow_depth < distance)
  {
    //light_diffuse_contribution  = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    //light_specular_contribution = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    light_diffuse_contribution  = vec4(vec3(light_view.z)*0.001f, 1.0f);
    light_specular_contribution = vec4(vec3(shadow_depth)*0.001f, 1.0f);
    return;
  }
  {
    //light_diffuse_contribution  = vec4(vec3(distance-length(shadow_world)*2), 1.0f);
    //light_specular_contribution = vec4(vec3(length(shadow_world)*0.001f), 1.0f);
    //return;
  }

  vec3 direction = normalize(world - light_position);
  float max_angle = (light_angle_falloff)/PI_4;
  float angle = (1.0 - dot(direction, light_direction))/(max_angle*0.15);
  float angle_intensity = clamp((1.0 - sqrt(angle))*1.0, 0.0f, 1.0);

  float intensity = ((light_intensity) / (distance*distance)) * angle_intensity;

  vec3 L = normalize(light_position - world);
  vec3 V = -normalize(camera_position - world);
  vec3 R = reflect(L,N);

  vec3 base_color = intensity * light_color;

	light_diffuse_contribution  = vec4(max(dot(N, L) , 0.0)*base_color, 1.0f);
	light_specular_contribution = vec4(max(dot(R, V), 0.0)*base_color, 1.0f);
}
