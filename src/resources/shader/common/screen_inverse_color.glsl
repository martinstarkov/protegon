#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	vec4 textureColor = texture(u_Texture, v_TexCoord);
	o_Color = vec4(1.0f - textureColor.r, 1.0f - textureColor.g, 1.0f - textureColor.b, textureColor.a) * v_Color;
}