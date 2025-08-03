R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec2 u_Resolution;
uniform vec2 u_LightPosition;
uniform vec4 u_Color;
uniform float u_LightIntensity;
uniform float u_LightRadius;
uniform float u_Falloff;
uniform vec3 u_AmbientColor;
uniform float u_AmbientIntensity;
//uniform vec3 u_LightAttenuation;

float sqr(float x) {
    return x * x;
}

float attenuate_no_cusp(float distance, float radius,
    float max_intensity, float falloff) {
    float s = distance / radius;

    if (s >= 1.0f)
        return 0.0f;

    float s2 = sqr(s);

    return max_intensity * sqr(1.0f - s2) / (1.0f + falloff * s2);
}

float attenuate_cusp(float distance, float radius,
    float max_intensity, float falloff) {
    float s = distance / radius;

    if (s >= 1.0f)
        return 0.0f;

    float s2 = sqr(s);

    return max_intensity * sqr(1.0f - s2) / (1.0f + falloff * s); // uses s instead of s2
}

void main() {
	vec2 pixel = gl_FragCoord.xy;
	pixel.y = u_Resolution.y - pixel.y;
	vec2 diff = u_LightPosition - pixel;
	float distance = length(diff);

    float attenuation = attenuate_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);

    // Various alternative light attenuation functions:
    // float attenuation = attenuate_no_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);
    // float attenuation = 1.0f / (u_LightAttenuation.x + u_LightAttenuation.y * distance + u_LightAttenuation.z * distance * distance);
    // float attenuation = 1.0f - distance * distance / (u_LightRadius * u_LightRadius);
    // float attenuation = 1.0f - smoothstep(0.0f, u_LightRadius, distance);
    // float attenuation = pow(clamp(1.0f - distance / u_LightRadius, 0.0f, 1.0f), 2.0f) * u_LightIntensity;

    vec4 total_light = vec4(u_Color.rgb * attenuation + u_AmbientColor.rgb * u_AmbientIntensity, attenuation + u_AmbientIntensity);
    o_Color = total_light * v_Color;
}
)"