R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main()
{
	/*
	// Naive method.
	vec4 tex = texture(u_Texture, v_TexCoord);
	float avg = (tex.x + tex.y + tex.z) / 3.0f;
	o_Color = vec4(avg, avg, avg, 1.0f);
	*/

	o_Color = texture(u_Texture, v_TexCoord);
    float average = 0.2126f * o_Color.r + 0.7152f * o_Color.g + 0.0722f * o_Color.b;
    o_Color = vec4(average, average, average, 1.0f);
}
)"