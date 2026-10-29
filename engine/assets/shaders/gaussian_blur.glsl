#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

// Horizontal: vec2(1.0, 0.0)
// Vertical:   vec2(0.0, 1.0)
uniform vec2 u_Direction;

// Scales sample spacing.
// 1.0 = standard texel spacing.
// 2.0 = wider blur, but can look more sparse.
// Usually prefer more blur passes over very large radius.
uniform float u_Radius;

const float weights[5] = float[](
	0.227027f,
	0.1945946f,
	0.1216216f,
	0.054054f,
	0.016216f
);

void main() {
	vec2 texelSize = 1.0f / vec2(textureSize(u_Texture, 0));
	vec2 offset = texelSize * u_Direction * u_Radius;

	vec4 result = texture(u_Texture, v_TexCoord) * weights[0];

	for (int i = 1; i < 5; ++i) {
		vec2 sampleOffset = offset * float(i);

		result += texture(u_Texture, v_TexCoord + sampleOffset) * weights[i];
		result += texture(u_Texture, v_TexCoord - sampleOffset) * weights[i];
	}

	o_Color = result * v_Color;
}