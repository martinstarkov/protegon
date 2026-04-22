#option auto_layout

#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_LocalCoord;
in vec4 v_ShapeData; // x = thickness, y = fade, z = start_angle, w = signed_aperture
flat in int v_EntityID;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float ArcDistance(vec2 point) {
    return 1.0 - length(point);
}

float WrapAngle(float a) {
    a = mod(a, TWO_PI);
    if (a < 0.0) a += TWO_PI;
    return a;
}

void main() {
    float thickness = v_ShapeData.x;
    float fade = v_ShapeData.y;
    float start_angle = WrapAngle(v_ShapeData.z);
    float signed_aperture = v_ShapeData.w;

    float aperture = abs(signed_aperture);
    bool clockwise = signed_aperture > 0.0;

    float angle = atan(v_LocalCoord.y, v_LocalCoord.x);
    angle = WrapAngle(angle);

    float relative_angle;
    if (clockwise) {
        relative_angle = WrapAngle(angle - start_angle);
    } else {
        relative_angle = WrapAngle(start_angle - angle);
    }

    if (aperture < TWO_PI && relative_angle > aperture)
        discard;

    float distance = ArcDistance(v_LocalCoord);

    float alpha = smoothstep(0.0, fade, distance);
    alpha *= smoothstep(thickness + fade, thickness, distance);

    if (alpha <= 0.0)
        discard;

    o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
    o_EntityID = v_EntityID;
}