R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main()
{
	vec4 textureColor = texture(u_Texture, v_TexCoord);
	o_Color = vec4(1.0 - textureColor.r, 1.0 - textureColor.g, 1.0 - textureColor.b, textureColor.a) * v_Color;
}
)"