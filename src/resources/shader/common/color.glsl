#option auto_layout

#type vertex

in vec3 a_Position;
in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main() {
	v_Color = a_Color;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}

#type fragment

out vec4 o_Color;

in vec4 v_Color;

void main() {
	o_Color = v_Color;
}