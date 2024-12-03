R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;
layout (location = 2) in float v_TexIndex;

uniform sampler2D u_Textures[8];

void main()
{
	vec4 texColor = v_Color;

	// Why? https://stackoverflow.com/a/74729081
	switch(int(v_TexIndex))
	{
		case  0: texColor *= texture(u_Textures[ 0], v_TexCoord); break;
		case  1: texColor *= texture(u_Textures[ 1], v_TexCoord); break;
		case  2: texColor *= texture(u_Textures[ 2], v_TexCoord); break;
		case  3: texColor *= texture(u_Textures[ 3], v_TexCoord); break;
		case  4: texColor *= texture(u_Textures[ 4], v_TexCoord); break;
		case  5: texColor *= texture(u_Textures[ 5], v_TexCoord); break;
		case  6: texColor *= texture(u_Textures[ 6], v_TexCoord); break;
		case  7: texColor *= texture(u_Textures[ 7], v_TexCoord); break;
	}

	if (texColor.a == 0.0)
		discard;

	o_Color = texColor;
}
)"