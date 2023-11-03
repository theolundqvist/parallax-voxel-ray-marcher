#version 410

// Trick from
// https://rauwendaal.net/2014/06/14/rendering-a-screen-covering-triangle-in-opengl/

out VS_OUT {
	vec2 texcoord;
} vs_out;

void main()
{
	float x = -1.0 + float((gl_VertexID & 1) << 2);
	float y = -1.0 + float((gl_VertexID & 2) << 1);

	vs_out.texcoord = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);

	gl_Position = vec4(x, y, 0.0, 1.0);
}
