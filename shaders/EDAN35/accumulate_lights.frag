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
  float cam_dist = length(camera_position - world); 

  vec4 light_view = lights[light_index].view_projection * vec4(world, 1.0f);
  vec3 light_proj = light_view.xyz / light_view.w;
  vec3 light_NDC = light_proj * 0.5f + 0.5f;

  float shadow = 0.0f;
  float bias = 0.000015f;
  int kernel_r = 2;
  for(int x = -kernel_r; x <= kernel_r; x++) {
    for(int y = -kernel_r; y <= kernel_r; y++) {
      float shadow_depth = texture(shadow_texture, light_NDC.xy+vec2(x,y)*shadowmap_texel_size).r;
      if(light_NDC.z - bias > shadow_depth)
      {
        shadow += 1.0f;
      }
    }
  }
  shadow /= ((kernel_r*2+1.0f)*(kernel_r*2+1.0f));


  float distance = length(world - light_position);
  vec3 direction = normalize(world - light_position);
  float max_angle = (light_angle_falloff)/PI_4;
  float angle = (1.0 - dot(direction, light_direction))/(max_angle*0.25);
  float angle_intensity = clamp((1.0 - sqrt(angle))*0.10, 0.0f, 1.0);

  float intensity = ((light_intensity*20.0f) / (pow(distance, 2))) * angle_intensity;
  if(light_index == 0){
    float angle_intensity = clamp((1.0 - sqrt(angle/1.3))*0.1, 0.0f, 1.0);
    intensity = (light_intensity*1.0f) * angle_intensity;
  }

  vec3 L = normalize(light_position - world);
  vec3 V = -normalize(camera_position - world);
  vec3 R = reflect(L,N);

  //vec3 base_color = light_color;  
  vec3 base_color = vec3(1.0f);  
  float max_intensity = 1.0f/max(base_color.r, max(base_color.g, base_color.b));
  base_color *= min(intensity, max_intensity);

  vec3 diffuse = vec3(max(dot(N, L), 0.0f)) * (1.0 - shadow);
  vec3 specular = vec3(max(dot(V, R), 0.0f)) * (1.0 - shadow);
	light_diffuse_contribution  = vec4(diffuse*base_color, 1.0f);
	light_specular_contribution = vec4(specular*base_color, 1.0f);
}
