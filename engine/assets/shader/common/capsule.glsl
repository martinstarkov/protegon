#option auto_layout

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_LocalCoord;
in vec4 v_ShapeData; // x = thickness, y = fade, z = normalized_radius
flat in int v_EntityID;

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
    float thickness    = v_ShapeData.x; // 0.0f = hollow, 1.0f = filled
    float fade         = v_ShapeData.y;
    float radius       = v_ShapeData.z;

    float distance = CapsuleDistance(v_LocalCoord, radius);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
    o_EntityID = v_EntityID;
}