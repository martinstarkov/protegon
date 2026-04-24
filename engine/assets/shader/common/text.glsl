#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;
flat in int v_EntityID;

uniform sampler2D u_Textures[{MAX_TEXTURE_SLOTS}];

uniform float u_Weight;       // default 0.5
uniform float u_Softness;     // default 0.1

uniform vec4 u_OutlineColor;  // alpha <= 0 disables
uniform vec2 u_Outline;       // x = width, y = softness

uniform vec4 u_GlowColor;     // alpha <= 0 disables
uniform vec2 u_Glow;          // x = outer width, y = softness

float Median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}

float Coverage(float distance, float weight, float softness) {
	float sd = distance - weight;
	float px = max(fwidth(sd), 0.0001f);
	return clamp((sd / px) / max(softness, 0.0001f) + 0.5f, 0.0f, 1.0f);
}

void main() {
	vec4 texColor = vec4(1.0f);

	{TEXTURE_SWITCH_BLOCK}

	float distance = Median(texColor.r, texColor.g, texColor.b);

	float fill = Coverage(distance, u_Weight, u_Softness);
	vec4 color = vec4(v_Color.rgb, v_Color.a * fill);

    if (u_Outline.x > 0.0f && u_OutlineColor.a > 0.0f) {
        float outline = Coverage(distance, u_Weight - u_Outline.x, u_Outline.y);
        float ring = max(outline - fill, 0.0f);

        vec4 outline_color = vec4(u_OutlineColor.rgb, u_OutlineColor.a * v_Color.a * ring);
        color.rgb = mix(outline_color.rgb, color.rgb, fill);
        color.a = max(color.a, outline_color.a);
    }

    if (u_Glow.x > 0.0f && u_GlowColor.a > 0.0f) {
        float glow = Coverage(distance, u_Weight - u_Glow.x, u_Glow.y);
        glow = max(glow - fill, 0.0f);

        vec4 glow_color = vec4(u_GlowColor.rgb, u_GlowColor.a * v_Color.a * glow);
        color.rgb = mix(color.rgb, glow_color.rgb, glow_color.a);
        color.a = max(color.a, glow_color.a);
    }

	if (color.a <= 0.0f) {
		discard;
	}

	o_Color = color;
	o_EntityID = v_EntityID;
}