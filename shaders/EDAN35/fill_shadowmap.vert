#version 410

struct ViewProjTransforms
{
	mat4 view_projection;
	mat4 view_projection_inverse;
};

layout (std140) uniform LightViewProjTransforms
{
	ViewProjTransforms lights[4];
};

uniform int light_index;
uniform mat4 vertex_model_to_world;

layout (location = 0) in vec3 vertex;
layout (location = 2) in vec3 texcoord;

out VS_OUT {
	vec2 texcoord;
} vs_out;

void main()
{
	vs_out.texcoord = texcoord.xy;

	gl_Position = lights[light_index].view_projection * vertex_model_to_world * vec4(vertex, 1.0);
}
