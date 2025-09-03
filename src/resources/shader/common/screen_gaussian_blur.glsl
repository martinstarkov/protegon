R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
    const float PI = 6.28318530718f; // 2 * Pi

    // === Gaussian Blur Settings ===
    const float directions = 16.0f; // Number of directions (higher = smoother blur)
    const float quality    = 3.0f;  // Samples per direction (higher = smoother blur)
    const float size       = 8.0f;  // Radius of blur
    // ==============================

	ivec2 texSize = textureSize(u_Texture, 0); // mip level 0
    vec2 radius = size / vec2(texSize.xy);
    vec4 color  = texture(u_Texture, v_TexCoord);

    // Accumulate blur samples
    for (float d = 0.0f; d < PI; d += PI / directions) {
        vec2 offsetDir = vec2(cos(d), sin(d));
        for (float i = 1.0f / quality; i <= 1.0f; i += 1.0f / quality) {
            color += texture(u_Texture, v_TexCoord + offsetDir * radius * i);
        }
    }

    // Normalize final color
    color /= (quality * directions - 15.0f); // Adjusted normalization factor
    o_Color = color * v_Color;
}
)"