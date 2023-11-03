#version 410

in VS_OUT {
	flat int axis_index;
} vs_out;

out vec4 frag_color;

void main()
{
	switch (vs_out.axis_index)
	{
	case 0:
		frag_color = vec4(0.8f, 0.2f, 0.2f, 1.0f);
		break;
	case 1:
		frag_color = vec4(0.2f, 0.8f, 0.2f, 1.0f);
		break;
	case 2:
		frag_color = vec4(0.2f, 0.2f, 0.8f, 1.0f);
		break;
	default:
		frag_color = vec4(1.0f);
		break;
	}
}
