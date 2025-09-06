#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = thickness, y = fade, z = normalized_radius, w = aspect_ratio

float RoundedRectDistance(vec2 point, vec2 half_size, float radius) {
    // r.xy = (point.x > 0.0f) ? r.xy : r.zw;
    // r.x  = (point.y > 0.0f) ? r.x  : r.y;

    float r = radius;
    
    vec2 q = abs(point) - half_size + r;
    
    float dist = min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) + r;

    return 1.0f - dist;
}

void main() {
    float thickness    = v_Data.x; // 0.0f = hollow, 1.0f = filled
    float fade         = v_Data.y;
    float radius       = v_Data.z;
    float aspect_ratio = v_Data.w;
    
    vec2 uv = v_TexCoord * 2.0f - 1.0f; // Normalize to: [-1, 1]

    float distance = RoundedRectDistance(uv, vec2(1.0f), radius);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
}