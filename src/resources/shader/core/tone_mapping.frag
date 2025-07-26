R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Exposure;
uniform float u_Gamma;

void main() {
    vec4 tex = texture(u_Texture, v_TexCoord);
	vec3 hdr_color = tex.rgb;

    // Alternative: Reinhard tone mapping
    // vec3 mapped = hdr_color / (hdr_color + vec3(1.0f));

    // Alternative: Exposure tone mapping
    vec3 mapped = vec3(1.0f) - exp(-hdr_color * u_Exposure);

    // Gamma correction. When this was enabled, black areas of the screen became red.
    mapped = pow(mapped, vec3(1.0f / u_Gamma));

	o_Color = vec4(mapped, tex.a);
}
)"