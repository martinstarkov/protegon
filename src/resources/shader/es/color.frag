R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;

void main() {
	o_Color = v_Color;
}
)"