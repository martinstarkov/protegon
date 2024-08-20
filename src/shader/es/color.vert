R"(#version 300 es
precision highp float;

in vec3 a_Position;
in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main()
{
	v_Color = a_Color / vec4(255.0);

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)"