#version 410

uniform bool has_opacity_texture;
uniform sampler2D opacity_texture;

in VS_OUT {
	vec2 texcoord;
} fs_in;

void main()
{
	if (has_opacity_texture && texture(opacity_texture, fs_in.texcoord).r < 1.0)
		discard;
}
