R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	vec4 texColor = v_Color;
	texColor *= texture(u_Texture, v_TexCoord);
	o_Color = texColor;
}
)"