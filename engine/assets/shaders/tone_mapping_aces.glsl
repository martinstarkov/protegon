#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Exposure;
uniform float u_Gamma;

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 ToneMapACES(vec3 color, float exposure) {
	return RRTAndODTFit(color * exposure);
}

void main() {
	vec4 tex_color = v_Color;
	tex_color *= texture(u_Texture, v_TexCoord);

    vec3 tone_mapped = ToneMapACES(tex_color.rgb, u_Exposure); 

    vec3 gamma_corrected = pow(tone_mapped, vec3(1.0f / u_Gamma));

	o_Color = vec4(gamma_corrected, tex_color.a);
}