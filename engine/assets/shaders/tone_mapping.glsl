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

void main() {
	vec4 tex_color = v_Color;
	tex_color *= texture(u_Texture, v_TexCoord);
	vec3 hdr_color = tex_color.rgb;

    // Alternative: Reinhard tone mapping
    // vec3 mapped = hdr_color / (hdr_color + vec3(1.0f));

    // Alternative: ACES filmic tone map
    // vec3 mapped = RRTAndODTFit(hdr_color * u_Exposure); 

    // Alternative: Exposure tone mapping
    vec3 mapped = vec3(1.0f) - exp(-hdr_color * u_Exposure);

    // Gamma correction.
    mapped = pow(mapped, vec3(1.0f / u_Gamma));

	o_Color = vec4(mapped, tex_color.a);
}