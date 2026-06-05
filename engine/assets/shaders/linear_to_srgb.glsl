#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Gamma;

void main() {
	vec4 tex_color = v_Color;
	tex_color *= texture(u_Texture, v_TexCoord);

    vec3 gamma_corrected = pow(tex_color.rgb, vec3(1.0f / u_Gamma));

	o_Color = vec4(gamma_corrected, tex_color.a);
}