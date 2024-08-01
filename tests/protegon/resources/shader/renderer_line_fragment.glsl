#version 330 core

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;

void main()
{
	o_Color = v_Color;
}