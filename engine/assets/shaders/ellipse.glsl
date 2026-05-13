#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_LocalCoord;
in vec4 v_ShapeData; // x = thickness, y = fade
flat in int v_EntityID;

float EllipseDistance(vec2 point, float radius) {
    return radius - length(point) / radius;
}

void main() {
    float thickness = v_ShapeData.x; // 0.0f = hollow, 1.0f = filled
    float fade = v_ShapeData.y;
    
    float radius = 1.0f;
    // Not technically an exact ellipse, for that see: https://iquilezles.org/articles/distfunctions2d/

    float distance = EllipseDistance(v_LocalCoord, radius);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
    o_EntityID = v_EntityID;
}