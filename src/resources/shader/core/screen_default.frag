R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main()
{
	o_Color = texture(u_Texture, v_TexCoord);
}
)"