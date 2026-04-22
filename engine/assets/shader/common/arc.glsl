#option auto_layout

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_LocalCoord;
in vec4 v_ShapeData; // x = thickness, y = fade, z = aperture, w = direction (positive = CW, negative = CCW)
flat in int v_EntityID;

const float PI = 3.14159265359f;

float ArcDistance(vec2 point) {
    return 1.0f - length(point);
}

void main() {
    float thickness = v_ShapeData.x; // 0.0f = hollow, 1.0f = filled
    float fade = v_ShapeData.y;
    float aperture = v_ShapeData.z;
    float direction = v_ShapeData.w;

    float angle = atan(v_LocalCoord.y, v_LocalCoord.x);
    if (angle < 0.0f) angle += 2.0f * PI;

    if (direction < 0.0f)
        angle = 2.0f * PI - angle;

    if (aperture < 2.0 * PI && angle > aperture)
        discard;

    float distance = ArcDistance(v_LocalCoord);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
    o_EntityID = v_EntityID;
}