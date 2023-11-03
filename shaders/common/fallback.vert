#version 410

layout (location = 0) in vec3 vertex;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

void main()
{
	gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(vertex, 1.0);
}
