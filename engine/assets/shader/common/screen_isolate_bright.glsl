#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Threshold; // brightness threshold, e.g. 1.0 for HDR, ~0.8 for LDR

void main() {

	vec4 tex = texture(u_Texture, v_TexCoord);

	// Perceptual luminance (linear space)
	float luminance =
		0.2126f * tex.r +
		0.7152f * tex.g +
		0.0722f * tex.b;

	// Keep only bright parts
	float mask = max(luminance - u_Threshold, 0.0);

	// Preserve color, suppress dark fragments
	o_Color = vec4(tex.rgb * mask, tex.a) * v_Color;
}
