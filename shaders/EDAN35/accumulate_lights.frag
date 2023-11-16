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


void main()
{
	vec2 shadowmap_texel_size = 1.0f / textureSize(shadow_texture, 0);

  vec2 pixel = gl_FragCoord.xy;
  vec3 N = normalize(texelFetch(normal_texture, ivec2(pixel), 0).xyz * 2.0f - 1.0f);
  float depth = texelFetch(depth_texture, ivec2(pixel), 0).r;

  vec2 NDC = (inverse_screen_resolution * pixel) * 2.0f - 1.0f;
  vec4 view = vec4(NDC, 1.0f, 1.0f);
  //view.xyz /= depth;
  vec4 world = camera.view_projection_inverse * view;

  
  

  vec3 L = normalize(light_position - world.xyz);
  vec3 V = normalize(camera_position - world.xyz);
  vec3 R = reflect(L, N);
	light_diffuse_contribution  = vec4(vec3(max(dot(N, L), 0.0)), 1.0f);
	light_specular_contribution = vec4(vec3(max(dot(V, R), 0.0)), 1.0f);
}
