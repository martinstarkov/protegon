R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;

uniform sampler2D u_Textures[16];

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
		case  8: texColor *= texture(u_Textures[ 8], v_TexCoord); break;
		case  9: texColor *= texture(u_Textures[ 9], v_TexCoord); break;
		case 10: texColor *= texture(u_Textures[10], v_TexCoord); break;
		case 11: texColor *= texture(u_Textures[11], v_TexCoord); break;
		case 12: texColor *= texture(u_Textures[12], v_TexCoord); break;
		case 13: texColor *= texture(u_Textures[13], v_TexCoord); break;
		case 14: texColor *= texture(u_Textures[14], v_TexCoord); break;
		case 15: texColor *= texture(u_Textures[15], v_TexCoord); break;
	}

	if (texColor.a == 0.0)
		discard;

	o_Color = texColor;
}
)"