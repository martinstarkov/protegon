R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

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

float sqr(float x)
{
    return x * x;
}

float attenuate_no_cusp(float distance, float radius,
    float max_intensity, float falloff)
{
    float s = distance / radius;

    if (s >= 1.0)
        return 0.0;

    float s2 = sqr(s);

    return max_intensity * sqr(1.0 - s2) / (1.0 + falloff * s2);
}

float attenuate_cusp(float distance, float radius,
    float max_intensity, float falloff)
{
    float s = distance / radius;

    if (s >= 1.0)
        return 0.0;

    float s2 = sqr(s);

    return max_intensity * sqr(1.0 - s2) / (1.0 + falloff * s);
}

void main() {
	vec2 pixel = gl_FragCoord.xy;
	pixel.y = u_Resolution.y - pixel.y;
	vec2 diff = u_LightPosition - pixel;
	float distance = length(diff);

    float attenuation = attenuate_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);

    // Various alternative light attenuation functions:
    // float attenuation = attenuate_no_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);
    // float attenuation = 1.0 / (u_LightAttenuation.x + u_LightAttenuation.y * distance + u_LightAttenuation.z * distance * distance);
    // float attenuation = clamp(1.0 - distance * distance / (u_LightRadius * u_LightRadius), 0.0, 1.0);

    o_Color = (attenuation * u_Color + u_AmbientIntensity * vec4(u_AmbientColor.x, u_AmbientColor.y, u_AmbientColor.z, 1.0)) * v_Color;
}
)"