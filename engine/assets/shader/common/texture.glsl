#option auto_layout

#type vertex

in vec3 a_Position;
in vec4 a_Color;
in vec2 a_TexCoord;
in float a_TexIndex;
in int a_EntityID;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
out float v_TexIndex;
flat out int v_EntityID;

void main() {
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;
flat in int v_EntityID;

uniform sampler2D u_Textures[{MAX_TEXTURE_SLOTS}];

void main() {
	vec4 texColor = v_Color;

    // Why? https://stackoverflow.com/a/74729081
	{TEXTURE_SWITCH_BLOCK}

	// v_TexIndex == 0 is the white texture, which SHOULD be drawn EVEN IF tinted transparent.
    if (texColor.a <= 0.0f && v_TexIndex != 0.0f)
        discard;

	o_Color = texColor;
	o_EntityID = v_EntityID;
}