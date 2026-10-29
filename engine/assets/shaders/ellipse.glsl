#option auto_layout

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_LocalCoord;
in vec4 v_ShapeData; // x = thickness, y = fade, z = radius_x, w = radius_y
flat in int v_EntityID;

float EllipseDistance(vec2 point, vec2 radii) {
    vec2 normalized = point / radii;
    float normalized_length = length(normalized);

    if (normalized_length <= 0.00001f) {
        return min(radii.x, radii.y);
    }

    float gradient = length(normalized / radii) / normalized_length;

    return (1.0f - normalized_length) / max(gradient, 0.00001f);
}

void main() {
    float thickness = v_ShapeData.x;
    float fade = v_ShapeData.y;
    vec2 radii = v_ShapeData.zw;

    float min_radius = min(radii.x, radii.y);

    float outer_distance = EllipseDistance(v_LocalCoord, radii);
    float alpha = smoothstep(0.0f, fade, outer_distance);

    if (thickness < min_radius) {
        vec2 inner_radii = radii - vec2(thickness);

        float inner_distance = EllipseDistance(v_LocalCoord, inner_radii);

        alpha *= 1.0f - smoothstep(0.0f, fade, inner_distance);
    }

    if (alpha <= 0.0f) {
        discard;
    }

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
    o_EntityID = v_EntityID;
}