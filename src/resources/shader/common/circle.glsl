#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = thickness, y = fade

float CircleDistance(vec2 point) {
    return 1.0f - length(point);
}

void main() {
    float thickness = v_Data.x; // 0.0f = hollow, 1.0f = filled
    float fade = v_Data.y;
    
    vec2 uv = v_TexCoord * 2.0f - 1.0f; // Normalize to: [-1, 1]
    // Not technically an exact ellipse, for that see: https://iquilezles.org/articles/distfunctions2d/

    float distance = CircleDistance(uv);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
}