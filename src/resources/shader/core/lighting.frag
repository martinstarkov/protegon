R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec2 u_Resolution;
uniform vec2 u_LightPos;
uniform vec3 u_AmbientColor;
uniform float u_LightIntensity;
uniform float u_AmbientIntensity;
uniform float u_LightRadius;

void main() {
	vec2 pixel = gl_FragCoord.xy;
	pixel.y = u_Resolution.y - pixel.y;
	vec2 diff = u_LightPos - pixel;
	float distance = length(diff);
    vec4 color = vec4(1.0, 1.0, 1.0, 1.0 / distance * u_LightIntensity);
	o_Color = color * v_Color;

	/*
	float distance = length(u_LightPos - vec2(gl_FragCoord.x, u_Resolution.y - gl_FragCoord.y));
	float attenuation = 1.0 / distance;
	vec4 falloff = vec4(attenuation, attenuation, attenuation, pow(attenuation, 3));
	vec3 light = clamp(v_Color * u_LightIntensity * falloff, 0.0, 1.0);

	vec4 pixel = texture(u_Texture, v_TexCoord);
	vec3 ambient = clamp(pixel.rgb * u_AmbientColor * u_AmbientIntensity, 0.0, 1.0); // TODO: Add shadows: + texture(u_OcclusionMask, v_TexCoord).rgb;

	o_Color = vec4(pixel.rgb * (ambient + light), 1.0) * v_Color;
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