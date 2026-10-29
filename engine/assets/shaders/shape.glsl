#option auto_layout

#type vertex

in vec3 a_Position;
in vec4 a_Color;
in vec2 a_LocalCoord;
in vec4 a_ShapeData;
in int a_EntityID;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_LocalCoord;
out vec4 v_ShapeData;
flat out int v_EntityID;

void main() {
	v_Color = a_Color;
	v_LocalCoord = a_LocalCoord;
	v_ShapeData = a_ShapeData;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}