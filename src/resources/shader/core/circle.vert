R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;
layout (location = 2) in vec3 a_LocalPosition;
layout (location = 3) in float a_Thickness;
layout (location = 4) in float a_Fade;

uniform mat4 u_ViewProjection;

layout (location = 0) out vec4 v_Color;
layout (location = 1) out vec3 v_LocalPosition;
layout (location = 2) out float v_Thickness;
layout (location = 3) out float v_Fade;

void main()
{
	v_Color = a_Color;
	v_LocalPosition = a_LocalPosition;
	v_Thickness = a_Thickness;
	v_Fade = a_Fade;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)"