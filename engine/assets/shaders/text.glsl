#type fragment

out vec4 o_Color;
out int o_EntityID;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;
flat in int v_EntityID;

uniform sampler2D u_Texture;

uniform float u_Weight;
uniform float u_Softness;

uniform vec4 u_OutlineColor;
uniform float u_OutlineWidth;
uniform float u_OutlineSoftness;

uniform vec4 u_ShadowColor;
uniform vec2 u_ShadowOffset;
uniform float u_ShadowWidth;
uniform float u_ShadowSoftness;

uniform vec4 u_OuterGlowColor;
uniform float u_OuterGlowWidth;
uniform float u_OuterGlowSoftness;

uniform vec4 u_InnerGlowColor;
uniform float u_InnerGlowWidth;
uniform float u_InnerGlowSoftness;

uniform float u_PixelRange;
uniform float u_IsDecoration;

float Median(vec3 value) {
	return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

float RawScreenPxRange() {
	vec2 texture_size = vec2(textureSize(u_Texture, 0));
	vec2 unit_range = vec2(u_PixelRange) / texture_size;
	vec2 screen_tex_size = vec2(1.0f) / fwidth(v_TexCoord);

	return 0.5f * dot(unit_range, screen_tex_size);
}

float ScreenPxRange() {
	return max(RawScreenPxRange(), 1.0f);
}

float EffectDistanceToScreenPx(float distance_px, float raw_screen_px_range) {
	return distance_px * raw_screen_px_range / max(u_PixelRange, 0.0001f);
}

vec4 SampleAtlas(vec2 uv) {
	return texture(u_Texture, uv);
}

float SampleMsdfDistance(vec2 uv) {
	return Median(SampleAtlas(uv).rgb);
}

float SampleSdfDistance(vec2 uv) {
	return SampleAtlas(uv).a;
}

float SignedDistancePx(float distance, float screen_px_range) {
	// Positive = inside glyph.
	// Negative = outside glyph.
	return screen_px_range * (distance - u_Weight);
}

float FillAlpha(float signed_distance_px, float softness_px) {
	float softness = max(softness_px, 0.0001f);
	return smoothstep(-softness, softness, signed_distance_px);
}

float ExpandedAlpha(float signed_distance_px, float width_px, float softness_px) {
	float width = max(width_px, 0.0f);
	float softness = max(softness_px, 0.0001f);

	return smoothstep(-width - softness, -width + softness, signed_distance_px);
}

float OutlineAlpha(float signed_distance_px, float fill_alpha, float width_px, float softness_px) {
	if (u_OutlineColor.a <= 0.0f || u_OutlineWidth <= 0.0f) {
		return 0.0f;
	}

	float expanded_alpha = ExpandedAlpha(signed_distance_px, width_px, softness_px);
	return clamp(expanded_alpha - fill_alpha, 0.0f, 1.0f);
}

float OuterGlowAlpha(
	float signed_distance_px,
	float fill_alpha,
	float width_px,
	float softness_px
) {
	if (u_OuterGlowColor.a <= 0.0f || u_OuterGlowWidth <= 0.0f) {
		return 0.0f;
	}

	float width = max(width_px, 0.0001f);
	float softness = clamp(softness_px, 0.0001f, width);

	float outside_distance = max(-signed_distance_px, 0.0f);
	float fade_start = max(width - softness, 0.0f);

	float alpha = 1.0f - smoothstep(fade_start, width, outside_distance);

	return alpha * (1.0f - fill_alpha);
}

float InnerGlowAlpha(
	float signed_distance_px,
	float fill_alpha,
	float width_px,
	float softness_px
) {
	if (u_InnerGlowColor.a <= 0.0f || u_InnerGlowWidth <= 0.0f) {
		return 0.0f;
	}

	float width = max(width_px, 0.0001f);
	float softness = clamp(softness_px, 0.0001f, width);

	float inside_distance = max(signed_distance_px, 0.0f);
	float fade_start = max(width - softness, 0.0f);

	float alpha = 1.0f - smoothstep(fade_start, width, inside_distance);

	return alpha * fill_alpha;
}

vec2 ScreenOffsetToTexOffset(vec2 offset_px) {
	return dFdx(v_TexCoord) * offset_px.x + dFdy(v_TexCoord) * offset_px.y;
}

vec4 Over(vec4 dst, vec4 src) {
	float out_alpha = src.a + dst.a * (1.0f - src.a);

	if (out_alpha <= 0.0f) {
		return vec4(0.0f);
	}

	vec3 out_rgb =
		(src.rgb * src.a + dst.rgb * dst.a * (1.0f - src.a)) / out_alpha;

	return vec4(out_rgb, out_alpha);
}

void main() {
	if (u_IsDecoration > 0.5f) {
		vec4 color = v_Color;

		if (color.a <= 0.0f) {
			discard;
		}

		o_Color = color;
		o_EntityID = v_EntityID;
		return;
	}

float raw_screen_px_range = RawScreenPxRange();
	float screen_px_range = max(raw_screen_px_range, 1.0f);

	float outline_width_px = EffectDistanceToScreenPx(u_OutlineWidth, raw_screen_px_range);
	float outline_softness_px = EffectDistanceToScreenPx(u_OutlineSoftness, raw_screen_px_range);

	float shadow_width_px = EffectDistanceToScreenPx(u_ShadowWidth, raw_screen_px_range);
	float shadow_softness_px = EffectDistanceToScreenPx(u_ShadowSoftness, raw_screen_px_range);

	float outer_glow_width_px = EffectDistanceToScreenPx(u_OuterGlowWidth, raw_screen_px_range);
	float outer_glow_softness_px = EffectDistanceToScreenPx(u_OuterGlowSoftness, raw_screen_px_range);

	float inner_glow_width_px = EffectDistanceToScreenPx(u_InnerGlowWidth, raw_screen_px_range);
	float inner_glow_softness_px = EffectDistanceToScreenPx(u_InnerGlowSoftness, raw_screen_px_range);

	float msdf_distance = SampleMsdfDistance(v_TexCoord);
	float sdf_distance = SampleSdfDistance(v_TexCoord);

	float fill_signed_px = SignedDistancePx(msdf_distance, screen_px_range);
	float effect_signed_px = SignedDistancePx(sdf_distance, screen_px_range);

	float fill_alpha = FillAlpha(fill_signed_px, u_Softness);

	vec4 color = vec4(0.0f);

	// 1. Shadow behind everything.
	if (u_ShadowColor.a > 0.0f) {
		vec2 shadow_uv = v_TexCoord - ScreenOffsetToTexOffset(u_ShadowOffset);

		float shadow_distance = SampleSdfDistance(shadow_uv);
		float shadow_signed_px = SignedDistancePx(shadow_distance, screen_px_range);

		float shadow_alpha = ExpandedAlpha(
			shadow_signed_px,
			shadow_width_px,
			shadow_softness_px
		);

		vec4 shadow_color = vec4(
			u_ShadowColor.rgb,
			u_ShadowColor.a * v_Color.a * shadow_alpha
		);

		color = Over(color, shadow_color);
	}

	// 2. Outer glow behind outline and fill.
	if (u_OuterGlowColor.a > 0.0f) {
		float outer_glow_alpha = OuterGlowAlpha(
			effect_signed_px,
			fill_alpha,
			outer_glow_width_px,
			outer_glow_softness_px
		);

		vec4 outer_glow_color = vec4(
			u_OuterGlowColor.rgb,
			u_OuterGlowColor.a * v_Color.a * outer_glow_alpha
		);

		color = Over(color, outer_glow_color);
	}

	// 3. Outline behind fill.
	if (u_OutlineColor.a > 0.0f) {
		float outline_alpha = OutlineAlpha(
			fill_signed_px,
			fill_alpha,
			outline_width_px,
			outline_softness_px
		);

		vec4 outline_color = vec4(
			u_OutlineColor.rgb,
			u_OutlineColor.a * v_Color.a * outline_alpha
		);

		color = Over(color, outline_color);
	}

	// 4. Main text fill.
	vec4 fill_color = vec4(v_Color.rgb, v_Color.a * fill_alpha);
	color = Over(color, fill_color);

	// 5. Inner glow over fill.
	if (u_InnerGlowColor.a > 0.0f) {
		float inner_glow_alpha = InnerGlowAlpha(
			effect_signed_px,
			fill_alpha,
			inner_glow_width_px,
			inner_glow_softness_px
		);

		vec4 inner_glow_color = vec4(
			u_InnerGlowColor.rgb,
			u_InnerGlowColor.a * v_Color.a * inner_glow_alpha
		);

		color = Over(color, inner_glow_color);
	}

	if (color.a <= 0.0f) {
		discard;
	}

	o_Color = color;
	o_EntityID = v_EntityID;
}