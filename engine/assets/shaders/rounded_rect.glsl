#option auto_layout

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_LocalCoord;
in vec4 v_ShapeData; // x = thickness, y = fade, z = normalized_radius, w = aspect_ratio
flat in int v_EntityID;

float RoundedRectDistance(vec2 point, vec2 half_size, float radius) {
    vec2 q = abs(point) - half_size + radius;

    float dist = min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) - radius;

    return -dist;
}

void main() {
    float thickness = v_ShapeData.x;
    float fade = v_ShapeData.y;
    float radius = v_ShapeData.z;
    float aspect_ratio = v_ShapeData.w;

    float distance = RoundedRectDistance(v_LocalCoord, vec2(1.0f, aspect_ratio), radius);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= 1.0f - smoothstep(thickness, thickness + fade, distance);

    if (alpha <= 0.0f) {
        discard;
    }

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
    o_EntityID = v_EntityID;
}
