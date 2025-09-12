#option auto_layout

#type vertex

in vec3 a_Position;
in vec4 a_Color;
in vec2 a_TexCoord;
in vec4 a_Data;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
out vec4 v_Data;

void main() {
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_Data = a_Data;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
}

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = texture index

uniform sampler2D u_Texture[{MAX_TEXTURE_SLOTS}];

void main() {
	vec4 texColor = v_Color;

	float v_TexIndex = v_Data.x;

    // Why? https://stackoverflow.com/a/74729081
	{TEXTURE_SWITCH_BLOCK}
    
    if (texColor.a == 0.0f) {
        discard;
    }

	o_Color = texColor;
}