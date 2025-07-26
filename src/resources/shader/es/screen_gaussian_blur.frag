R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec2 u_Resolution;

void main() {
    const float PI = 6.28318530718f; // 2 * Pi

    // === Gaussian Blur Settings ===
    const float directions = 16.0f; // Number of directions (higher = smoother blur)
    const float quality    = 3.0f;  // Samples per direction (higher = smoother blur)
    const float size       = 8.0f;  // Radius of blur
    // ==============================

    vec2 radius = size / u_Resolution;
    vec2 uv     = gl_FragCoord.xy / u_Resolution;
    vec4 color  = texture(u_Texture, uv);

    // Accumulate blur samples
    for (float d = 0.0f; d < PI; d += PI / directions) {
        vec2 offsetDir = vec2(cos(d), sin(d));
        for (float i = 1.0f / quality; i <= 1.0f; i += 1.0f / quality) {
            color += texture(u_Texture, uv + offsetDir * radius * i);
        }
    }

    // Normalize final color
    color /= (quality * directions - 15.0f); // Adjusted normalization factor
    o_Color = color * v_Color;
}
)"