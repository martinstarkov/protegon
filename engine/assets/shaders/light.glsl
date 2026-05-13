#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_TexCoord;
flat in int v_EntityID;

uniform float u_UseCone;
uniform float u_ConeAngle;
uniform vec4 u_Color;
uniform float u_LightIntensity;
uniform float u_LightRadius;
uniform float u_Falloff;
uniform vec3 u_AmbientColor;
uniform float u_AmbientIntensity;
uniform vec3 u_LightAttenuation;

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

float sdPie(vec2 p, vec2 c, float r)
{
    p.x = abs(p.x);
    float l = length(p) - r;
    float m = length(p - c * clamp(dot(p, c), 0.0f, r));
    return max(l, m * sign(c.y * p.x - c.x * p.y));
}

void main() {
	vec2 uv = v_TexCoord;
    vec2 center = vec2(0.5f, 0.5f);
	vec2 diff = center - uv;
	float distance = length(diff);

    vec2 p = uv - center;
    p = vec2(p.y, p.x);
    
    float circleSdf = distance;
    vec2 angles = vec2(sin(u_ConeAngle), cos(u_ConeAngle));
    float coneSdf = sdPie(p, angles, 0.5f);
    
    float alphaCone = 1.0 - smoothstep(0.0, 0.005, coneSdf);

    float alphaCircle =
        smoothstep(0.0, 0.005, circleSdf) *
        smoothstep(1.0 + 0.005, 1.0, circleSdf);

    float alpha = mix(alphaCircle, alphaCone, u_UseCone);


    if (alpha <= 0.0f)
        discard;

    float attenuation = attenuate_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);

    // Various alternative light attenuation functions:
    //float attenuation = attenuate_no_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);
    //float attenuation = 1.0f / (u_LightAttenuation.x + u_LightAttenuation.y * distance + u_LightAttenuation.z * distance * distance);
    //float attenuation = 1.0f - distance * distance / (u_LightRadius * u_LightRadius);
    //float attenuation = 1.0f - smoothstep(0.0f, u_LightRadius, distance);
    //float attenuation = pow(clamp(1.0f - distance / u_LightRadius, 0.0f, 1.0f), 2.0f) * u_LightIntensity;

    vec4 total_light = vec4(u_Color.rgb * attenuation + u_AmbientColor.rgb * u_AmbientIntensity, (attenuation + u_AmbientIntensity) * alpha);
    
    o_Color = total_light * v_Color;
    o_EntityID = v_EntityID;
}