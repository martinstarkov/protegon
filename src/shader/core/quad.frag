R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;
layout (location = 2) in float v_TexIndex;
layout (location = 3) in float v_TilingFactor;

uniform sampler2D u_Textures[32];

void main()
{
	vec4 texColor = v_Color;
	int index = int(v_TexIndex);

	texColor *= texture(u_Textures[index], v_TexCoord * v_TilingFactor);

	if (texColor.a == 0.0)
		discard;

	o_Color = texColor;
}
)"