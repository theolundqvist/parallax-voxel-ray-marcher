#version 410

layout (location = 0) in vec3 vertex;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;
uniform float thickness_scale;
uniform float length_scale;

out VS_OUT {
	flat int axis_index;
} vs_out;


void main()
{
	vs_out.axis_index = gl_InstanceID;
	vec3 local_vertex = vertex;

	// One arrow is instanced three times, to provide all three axes. That
	// arrow represents a (1 0 0) vector.

	// Scale the arrow before it is transformed to represent an axis other
	// than x.
	local_vertex.yz *= thickness_scale;
	local_vertex.x *= length_scale;

	// The first instance is for the x axis, so no additional
	// transformation is needed.
	//
	// The second instance is for the y axis, so swap the x and y
	// components.
	if (vs_out.axis_index == 1)
	{
		local_vertex.xy = local_vertex.yx;
	}
	// The third instance is for the z axis, so set x to -z, and z to x.
	else if (vs_out.axis_index == 2)
	{
		float tmpX = local_vertex.x;
		local_vertex.x = -local_vertex.z;
		local_vertex.z = tmpX;
	}

	gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(local_vertex, 1.0);
}
