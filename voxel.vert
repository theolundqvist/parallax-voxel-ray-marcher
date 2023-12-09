
#version 410
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tex_coord_in;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

uniform mat4 vertex_model_to_world;
uniform mat4 normal_model_to_world;
uniform mat4 vertex_world_to_clip;
uniform int has_textures;
uniform int has_diffuse_texture;
uniform int has_opacity_texture;

uniform vec3 camera_position;

out vec3 fV;
out vec3 pos;
out vec3 world_pos;

void main()
{

	pos = tex_coord_in;
	vec3 worldPos = (vertex_model_to_world*vec4(vertex,1)).xyz;
	world_pos=worldPos;
	fV = camera_position - worldPos;
	gl_Position = vertex_world_to_clip*vec4(worldPos,1);

}
