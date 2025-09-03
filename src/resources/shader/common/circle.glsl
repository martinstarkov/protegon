#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;

void main() {
    float fade = 0.005f;
    // 1.0f for filled, 0.0f for hollow.
    float thickness = v_TexIndex;
    // Calculate distance and fill circle with white
    vec2 local_pos = v_TexCoord * vec2(2.0f) - vec2(1.0f);
    float distance = 1.0f - length(local_pos);
    float circle = smoothstep(0.0f, fade, distance);
    circle *= smoothstep(thickness + fade, thickness, distance);

	if (circle == 0.0f)
		discard;

    // Set output color
    o_Color = v_Color;
	o_Color.a *= circle;
}