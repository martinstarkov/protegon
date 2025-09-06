#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = thickness, y = fade, z = aperture, w = direction (positive = CW, negative = CCW)

const float PI = 3.14159265359f;

float ArcDistance(vec2 point) {
    return 1.0f - length(point);
}

void main() {
    float thickness = v_Data.x; // 0.0f = hollow, 1.0f = filled
    float fade = v_Data.y;
    float aperture = v_Data.z;
    float direction = v_Data.w;
    
    vec2 uv = v_TexCoord * 2.0f - 1.0f; // Normalize to: [-1, 1]

    float angle = atan(uv.y, uv.x);
    if (angle < 0.0f) angle += 2.0f * PI;

    if (direction < 0.0f) angle = 2.0f * PI - angle;

    // Check if angle is inside the aperture range [0, aperture]
    if (angle > aperture)
        discard;

    float distance = ArcDistance(uv);

    float alpha = smoothstep(0.0f, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0f)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
}