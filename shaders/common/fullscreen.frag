#version 410

in VS_OUT {
	vec2 texcoord;
} fs_in;

out vec4 result;

uniform sampler2D tex;
uniform ivec4 swizzle;
uniform bool linearise;
uniform float near;
uniform float far;

float lineariseDepth(float value)
{
	return (2.0 * near) / (far + near - value * (far - near));
}

void main()
{
	vec4 value = texture(tex, fs_in.texcoord);
	for (int i = 0; i < 4; ++i)
		result[i] = (0 <= swizzle[i]) && (swizzle[i] <= 3)
		          ? (linearise ? lineariseDepth(value[swizzle[i]]) : value[swizzle[i]])
		          : 1.0;
}
