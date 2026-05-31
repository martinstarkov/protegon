#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform sampler2D u_Additive;

uniform float u_Intensity;
uniform vec4 u_Tint;

void main() {
	vec4 scene = texture(u_Texture, v_TexCoord);
	vec4 additive = texture(u_Additive, v_TexCoord);

	vec3 added = additive.rgb * u_Tint.rgb * u_Tint.a * u_Intensity;

	o_Color = vec4(scene.rgb + added, additive.a + scene.a) * v_Color;
}