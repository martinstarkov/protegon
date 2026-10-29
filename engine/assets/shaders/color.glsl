#option auto_layout

#type vertex

in vec3 a_Position;
in vec4 a_Color;
in int a_EntityID;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
flat out int v_EntityID;

void main() {
	v_Color = a_Color;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
flat in int v_EntityID;

void main() {
	o_Color = v_Color;
	o_EntityID = v_EntityID;
}