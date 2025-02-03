R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;
layout (location = 2) in float v_TexIndex;

void main()
{
    float fade = 0.005f;
    // 1.0f for filled, 0.0f for hollow.
    float thickness = 1.0f;
    // Calculate distance and fill circle with white
    vec2 local_pos = v_TexCoord * vec2(2.0f) - vec2(1.0f);
    float distance = 1.0f - length(local_pos);
    float circle = smoothstep(0.0, fade, distance);
    circle *= smoothstep(thickness + fade, thickness, distance);

	if (circle == 0.0)
		discard;

    // Set output color
    o_Color = v_Color;
	o_Color.a *= circle;
}
)"