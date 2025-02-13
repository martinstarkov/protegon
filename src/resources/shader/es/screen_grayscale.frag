R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main()
{
	/*
	// Naive method.
	vec4 tex = texture(u_Texture, v_TexCoord);
	float avg = (tex.x + tex.y + tex.z) / 3.0f;
	o_Color = vec4(avg, avg, avg, 1.0f);
	*/

	// From wikipedia:
	o_Color = texture(u_Texture, v_TexCoord);
    float average = 0.2126f * o_Color.r + 0.7152f * o_Color.g + 0.0722f * o_Color.b;
    o_Color = vec4(average, average, average, 1.0f) * v_Color;
}
)"