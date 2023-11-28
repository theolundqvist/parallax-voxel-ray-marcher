
#version 410
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tex_coord_in;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

uniform mat4 model_to_world;
uniform mat4 world_to_model;
uniform mat4 vertex_world_to_clip;

uniform vec3 camera_position;

out vec3 fV;
out vec3 pos;

void main()
{
	pos = tex_coord_in;
	vec3 worldPos = (model_to_world*vec4(vertex,1)).xyz;
	vec3 V = camera_position - worldPos;
	fV = (world_to_model * vec4(V, 1)).xyz;
	gl_Position = vertex_world_to_clip*vec4(worldPos,1);

}
