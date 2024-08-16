R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in float a_TexIndex;
layout (location = 4) in float a_TilingFactor;

uniform mat4 u_ViewProjection;

layout (location = 0) out vec4 v_Color;
layout (location = 1) out vec2 v_TexCoord;
layout (location = 2) out float v_TexIndex;
layout (location = 3) out float v_TilingFactor;

void main()
{
	v_Color = a_Color / vec4(255.0);
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)"