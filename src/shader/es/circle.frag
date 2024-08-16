R"(#version 300 es
precision mediump float;

out vec4 o_Color;

in vec3 v_LocalPosition;
in vec4 v_Color;
in float v_Thickness;
in float v_Fade;

void main()
{
    // Calculate distance and fill circle with white
    float distance = 1.0 - length(v_LocalPosition);
    float circle = smoothstep(0.0, v_Fade, distance);
    circle *= smoothstep(v_Thickness + v_Fade, v_Thickness, distance);

	if (circle == 0.0)
		discard;

    // Set output color
    o_Color = v_Color;
	o_Color.a *= circle;
}
)"