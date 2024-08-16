R"(#version 300 es
precision mediump float;

in vec3 a_Position;
in vec4 a_Color;
in vec2 a_TexCoord;
in float a_TexIndex;
in float a_TilingFactor;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
out float v_TexIndex;
out float v_TilingFactor;

void main()
{
	v_Color = a_Color / vec4(255.0);
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)"