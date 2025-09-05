#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = border_thickness

void main() {
    float fade = 0.005f;

    float border_thickness = v_Data.x; // 0.0f = hollow, 1.0f = filled
    
    vec2 uv = v_TexCoord * 2.0f - 1.0f; // Normalize to: [-1, 1]
    
    float distance = 1.0f - length(uv);
    
    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(border_thickness + fade, border_thickness, distance);

	if (alpha <= 0.0f)
		discard;

    // Set output color
    o_Color = v_Color;
	o_Color.a *= alpha;
}