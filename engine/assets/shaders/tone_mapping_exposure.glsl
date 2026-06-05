#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Exposure;
uniform float u_Gamma;

vec3 ToneMapExposure(vec3 color, float exposure) {
	return vec3(1.0f) - exp(-color * exposure);
}

void main() {
	vec4 tex_color = v_Color;
	tex_color *= texture(u_Texture, v_TexCoord);

    vec3 tone_mapped =  ToneMapExposure(tex_color.rgb, u_Exposure);

    vec3 gamma_corrected = pow(tone_mapped, vec3(1.0f / u_Gamma));

	o_Color = vec4(gamma_corrected, tex_color.a);
}