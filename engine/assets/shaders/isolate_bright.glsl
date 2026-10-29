#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

uniform float u_Threshold;
uniform float u_SoftKnee;

float Luminance(vec3 color) {
	return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

void main() {
	vec4 tex = texture(u_Texture, v_TexCoord);

	float brightness = Luminance(tex.rgb);

	// Soft threshold:
	// below u_Threshold -> 0
	// above u_Threshold + u_SoftKnee -> 1
	float mask = smoothstep(u_Threshold, u_Threshold + u_SoftKnee, brightness);

	vec3 extracted = tex.rgb * mask;

	o_Color = vec4(extracted, tex.a * mask) * v_Color;
}