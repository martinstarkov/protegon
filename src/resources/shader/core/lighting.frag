R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec2 u_Resolution;
uniform vec2 u_LightPos;
uniform float u_LightIntensity;
uniform vec3 u_LightAttenuation;
uniform float u_LightRadius;
uniform float u_Compression;
uniform float u_Falloff;
uniform vec3 u_AmbientColor;
uniform float u_AmbientIntensity;

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

    return max_intensity * sqr(1 - s2) / (1 + falloff * s2);
}

float attenuate_cusp(float distance, float radius,
    float max_intensity, float falloff)
{
    float s = distance / radius;

    if (s >= 1.0)
        return 0.0;

    float s2 = sqr(s);

    return max_intensity * sqr(1 - s2) / (1 + falloff * s);
}

void main() {
	vec2 pixel = gl_FragCoord.xy;
	pixel.y = u_Resolution.y - pixel.y;
	vec2 diff = u_LightPos - pixel;
	float distance = length(diff);

    //float attenuation = pow(smoothstep(u_LightRadius, 0, distance), u_Compression);

    //float attenuation = 1.0 / (u_LightAttenuation.x + u_LightAttenuation.y * distance + u_LightAttenuation.z * distance * distance);
    
    //float attenuation = clamp(1.0 - distance*distance/(u_LightRadius*u_LightRadius), 0.0, 1.0);

    float attenuation = attenuate_cusp(distance, u_LightRadius, u_LightIntensity, u_Falloff);

    vec4 color = (attenuation * v_Color + u_AmbientIntensity * vec4(u_AmbientColor.x, u_AmbientColor.y, u_AmbientColor.z, 1.0));
    o_Color = color; // * texture(u_Texture, v_TexCoord);

    /*
	vec2 pixel = gl_FragCoord.xy;
	pixel.y = u_Resolution.y - pixel.y;
	vec2 diff = u_LightPos - pixel;
	float distance = length(diff);
    float attenuation = 1.0 / distance;
    
    vec4 color = vec4(1.0, 1.0, 1.0, pow(attenuation, 0.9) * u_LightIntensity);
	o_Color = color * v_Color;
    */

    /*	
	vec4 falloff = vec4(attenuation, attenuation, attenuation, pow(attenuation, 3));
	vec3 light = clamp(v_Color * u_LightIntensity * falloff, 0.0, 1.0).rgb;

	vec4 texture_pixel = texture(u_Texture, v_TexCoord);
	vec3 ambient = clamp(texture_pixel.rgb * u_AmbientColor * u_AmbientIntensity, 0.0, 1.0); // TODO: Add shadows: + texture(u_OcclusionMask, v_TexCoord).rgb;

	o_Color = vec4(texture_pixel.rgb * (ambient + light), 1.0) * v_Color;
    */
}

/*

// TODO: Add optional normal maps:

uniform float distance;
uniform float intensity;
uniform float opacity;
uniform vec2 angleRange;
uniform vec4 normalVisibility;
uniform sampler2D normals;
uniform sampler2D colors;

in vec3 positionOS;
in vec4 positionPS;
in vec2 positionWS;
in vec2 centerWS;

void main() {
    vec2 positionSS = positionPS.xy / positionPS.w * 0.5 + 0.5;
    vec4 normal = texture(normals, positionSS);
    vec4 color = texture(colors, positionSS);

    float length = length(positionOS.xy);
    float distanceFalloff = clamp(1.0 - length * 2.0 + distance, 0.0, 1.0);
    distanceFalloff *= distanceFalloff;

    vec2 direction = positionOS.xy / length;
    float angle = acos(dot(direction, vec2(0.99, 0.0)));
    float angularFalloff = clamp(smoothstep(angleRange.x, angleRange.y, angle), 0.0, 1.0);

    vec2 normalVector = normal.xy * 2.0 - 1.0;

    direction = normalize(positionWS - centerWS);
    float strength = clamp(dot(-direction, normalVector), 0.0, 1.0);
    strength = mix(1.0, strength, normal.a * normalVisibility.y * normal.b);

    float finalA = angularFalloff * distanceFalloff * intensity * strength;
    vec3 finalColor = mix(vec3(1.0), color.rgb, normalVisibility.z);
    float originalA = finalA;
    finalA *= mix(1.0, color.a, opacity);

    float normalA = normal.a * step(positionSS.y, normalVisibility.x);

    gl_FragColor.rgb = finalColor * v_Color;
    gl_FragColor.a = finalA;

    gl_FragColor.rgba += mix(
        vec4(v_Color * finalA * normalVisibility.w, 0.0),
        vec4(v_Color * originalA * normalVisibility.w, originalA),
        opacity
    );
}

*/

)"