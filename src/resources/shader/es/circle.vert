R"(#version 300 es
precision highp float;

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_LocalPosition;
layout (location = 2) in vec4 a_Color;
layout (location = 3) in float a_Thickness;
layout (location = 4) in float a_Fade;

uniform mat4 u_ViewProjection;

out vec3 v_LocalPosition;
out vec4 v_Color;
out float v_Thickness;
out float v_Fade;

void main()
{
	v_LocalPosition = a_LocalPosition;
	v_Color = a_Color;
	v_Thickness = a_Thickness;
	v_Fade = a_Fade;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)"