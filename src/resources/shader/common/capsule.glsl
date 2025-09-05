#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec4 v_Data; // x = border_thickness, y = radius, z = width, w = height

float CapsuleSegment(vec2 p, vec2 a, vec2 b) {
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
    return length(pa - h * ba);
}

void main() {
    float fade = 0.005f;
    float border_thickness = v_Data.x; // 0.0f = hollow, 1.0f = filled
    float radius            = v_Data.y;
    float width             = v_Data.z;
    float height            = v_Data.w;

    // UV to object space [-1, 1]
    vec2 uv = v_TexCoord * 2.0f - 1.0f;
    vec2 p = uv * vec2(width, height) * 0.5f;

    // Capsule spine (excluding semicircle caps)
    vec2 a = vec2(-width * 0.5f + radius, 0.0f);
    vec2 b = vec2( width * 0.5f - radius, 0.0f);

    // Distance from p to capsule edge
    float distance = 1.0f - CapsuleSegment(p, a, b) / radius;

    float capsule = smoothstep(0.0f, fade, distance);
    capsule *= smoothstep(border_thickness + fade, border_thickness, distance);

    if (capsule <= 0.0f)
        discard;

    o_Color = v_Color;
    o_Color.a *= capsule;
}
