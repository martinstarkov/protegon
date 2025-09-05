#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = thickness, y = fade, z = normalized_radius, w = aspect_ratio

float CapsuleDistance(vec2 point, float radius) {
    vec2 a = vec2(-1.0f + radius, 0.0f);
    vec2 b = vec2( 1.0f - radius, 0.0f);

    vec2 ba = b - a;
    vec2 pa = point - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);

    vec2 p = pa - h * ba;

    return 1.0f - length(p) / radius;
}

void main() {
    float thickness    = v_Data.x; // 0.0f = hollow, 1.0f = filled
    float fade         = v_Data.y;
    float radius       = v_Data.z;
    float aspect_ratio = v_Data.w;
    
    vec2 uv = v_TexCoord * 2.0f - 1.0f; // Normalize to: [-1, 1]
    uv.y *= aspect_ratio;

    float distance = CapsuleDistance(uv, radius);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
}
