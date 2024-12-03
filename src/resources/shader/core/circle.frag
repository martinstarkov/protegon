R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec3 v_LocalPosition;
layout (location = 2) in float v_Thickness;
layout (location = 3) in float v_Fade;

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