R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main()
{
	vec4 textureColor = texture(u_Texture, v_TexCoord);
	o_Color = vec4(1.0 - textureColor.r, 1.0 - textureColor.g, 1.0 - textureColor.b, textureColor.a) * v_Color;
}
)"