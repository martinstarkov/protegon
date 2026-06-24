#include "renderer/text/text_layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/intersect.h"
#include "core/math/overlap.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "renderer/text/text_style.h"

namespace ptgn {

namespace {

constexpr int kShrinkScaleSearchIterations{ 16 };

struct GlyphEffectOscillation {
	V2_float frequency_multiplier{ 1.0f, 1.0f };
	V2_float glyph_phase_multiplier;
};

constexpr float kGlyphEffectPhaseStep{ 0.35f };

constexpr GlyphEffectOscillation kWobbleOscillation{ .frequency_multiplier{ 1.0f, 1.37f } };

constexpr GlyphEffectOscillation kShakeOscillation{
	.frequency_multiplier{ 17.0f, 23.0f },
	.glyph_phase_multiplier{ 12.9898f, 78.233f },
};

enum class LineFlushReason {
	SoftWrap,
	ExplicitNewline,
	EndOfText
};

struct RichTextToken {
	enum class Type : std::uint8_t {
		Word,
		Space,
		Tab,
		Newline,
	};

	Type type{ Type::Word };
	std::u32string text;
	std::size_t source_codepoint_begin{ 0 };
	std::size_t run_index{ 0 };
	float width{ 0.0f };

	constexpr bool IsSpacing() const {
		return type == Type::Space || type == Type::Tab;
	}
};

struct ResolvedGlyph {
	std::uint32_t codepoint{ 0 };
	impl::GlyphMetrics metrics;
	GlyphRenderStyle render_style;
	std::size_t source_run_index{ 0 };
	std::size_t source_codepoint_index{ 0 };
	impl::TextureId texture{ 0 };
};

struct TextLineMetrics {
	float height{ 0.0f };
	float ascent{ 0.0f };
	float descent{ 0.0f };
};

DistanceFieldStyle ResolveDistanceFieldStyle(
	const impl::FontAtlas& font, const TextRunStyle& style
) {
	auto sdf{ style.sdf };

	sdf.pixel_range = font.GetMetrics().pixel_range;

	if (HasFlag(style.flags, FontStyle::Bold) && style.fake_bold_if_missing) {
		sdf.weight -= style.fake_bold_weight;
	}

	sdf.weight = std::clamp(sdf.weight, 0.0f, 1.0f);

	return sdf;
}

std::vector<TextBatchStyle> BuildBatchStyles(const impl::ResolvedStyledText& styled_text) {
	std::vector<TextBatchStyle> styles;
	styles.reserve(styled_text.runs.size());

	for (const auto& run : styled_text.runs) {
		PTGN_ASSERT(run.font, "Valid font required for text run");
		styles.emplace_back(
			TextBatchStyle{
				.texture = run.font->GetTexture(),
				.sdf	 = ResolveDistanceFieldStyle(*run.font, run.style),
			}
		);
	}

	return styles;
}

struct TextLayoutBuildContext {
	TextLayoutBuildContext(
		const impl::ResolvedStyledText& styled_text, const TextBox& box, float global_shrink
	) :
		styled_text{ styled_text },
		box{ box },
		global_shrink{ global_shrink },
		wrap_width{ box.rect.GetSize().x },
		can_wrap{ box.style.wrap_mode != WrapMode::None && wrap_width > 0.0f } {
		layout.used_shrink_scale = global_shrink;
		layout.batch_styles		 = BuildBatchStyles(styled_text);
	}

	const impl::ResolvedStyledText& styled_text;
	const TextBox& box;

	float global_shrink{ 1.0f };
	float wrap_width{ 0.0f };
	bool can_wrap{ false };

	TextLayout layout;
	std::vector<Glyph> current_line_glyphs;

	V2_float current_line_size;
	float current_line_ascent{ 0.0f };
	float current_line_descent{ 0.0f };
	float y{ 0.0f };

	std::size_t visible_order{ 0 };
};

bool IsWhitespace(std::uint32_t codepoint) {
	return codepoint == U' ' || codepoint == U'\t' || codepoint == U'\n' || codepoint == U'\r';
}

std::u32string DecodeUtf8(std::string_view text) {
	constexpr unsigned char kAsciiMask{ 0x80u };

	constexpr unsigned char kTwoByteMask{ 0xE0u };
	constexpr unsigned char kTwoByteLead{ 0xC0u };
	constexpr unsigned char kTwoBytePayloadMask{ 0x1Fu };

	constexpr unsigned char kThreeByteMask{ 0xF0u };
	constexpr unsigned char kThreeByteLead{ 0xE0u };
	constexpr unsigned char kThreeBytePayloadMask{ 0x0Fu };

	constexpr unsigned char kFourByteMask{ 0xF8u };
	constexpr unsigned char kFourByteLead{ 0xF0u };
	constexpr unsigned char kFourBytePayloadMask{ 0x07u };

	constexpr unsigned char kContinuationPayloadMask{ 0x3Fu };

	constexpr int kShift6{ 6 };
	constexpr int kShift12{ 12 };
	constexpr int kShift18{ 18 };

	std::u32string out;
	out.reserve(text.size());

	auto i{ 0uz };
	while (i < text.size()) {
		unsigned char c{ static_cast<unsigned char>(text[i]) };

		if ((c & kAsciiMask) == 0u) {
			out.push_back(static_cast<char32_t>(c));
			++i;
			continue;
		}

		if ((c & kTwoByteMask) == kTwoByteLead && i + 1 < text.size()) {
			char32_t cp{ static_cast<char32_t>(
				((c & kTwoBytePayloadMask) << kShift6) |
				(static_cast<unsigned char>(text[i + 1]) & kContinuationPayloadMask)
			) };
			out.push_back(cp);
			i += 2;
			continue;
		}

		if ((c & kThreeByteMask) == kThreeByteLead && i + 2 < text.size()) {
			char32_t cp{ static_cast<char32_t>(
				((c & kThreeBytePayloadMask) << kShift12) |
				((static_cast<unsigned char>(text[i + 1]) & kContinuationPayloadMask) << kShift6) |
				(static_cast<unsigned char>(text[i + 2]) & kContinuationPayloadMask)
			) };
			out.push_back(cp);
			i += 3;
			continue;
		}

		if ((c & kFourByteMask) == kFourByteLead && i + 3 < text.size()) {
			char32_t cp{ static_cast<char32_t>(
				((c & kFourBytePayloadMask) << kShift18) |
				((static_cast<unsigned char>(text[i + 1]) & kContinuationPayloadMask) << kShift12) |
				((static_cast<unsigned char>(text[i + 2]) & kContinuationPayloadMask) << kShift6) |
				(static_cast<unsigned char>(text[i + 3]) & kContinuationPayloadMask)
			) };
			out.push_back(cp);
			i += 4;
			continue;
		}

		out.push_back(U'?');
		++i;
	}

	return out;
}

void ReindexVisibleGlyphs(TextLayout& layout) {
	std::size_t visible_order{ 0 };

	for (auto& glyph : layout.glyphs) {
		if (!glyph.visible) {
			continue;
		}

		glyph.visible_order = visible_order++;
	}
}

void RecalculateLayoutGeometry(TextLayout& layout) {
	layout.measured_size = {};
	layout.bounds		 = {};

	if (layout.lines.empty()) {
		return;
	}

	bool found_bounds{ false };

	for (const auto& line : layout.lines) {
		layout.measured_size.x = std::max(layout.measured_size.x, line.size.x);

		layout.measured_size.y += line.size.y;

		if (!found_bounds) {
			layout.bounds = line.bounds;
			found_bounds  = true;
			continue;
		}

		layout.bounds.min.x = std::min(layout.bounds.min.x, line.bounds.min.x);

		layout.bounds.min.y = std::min(layout.bounds.min.y, line.bounds.min.y);

		layout.bounds.max.x = std::max(layout.bounds.max.x, line.bounds.max.x);

		layout.bounds.max.y = std::max(layout.bounds.max.y, line.bounds.max.y);
	}
}

float MeasureRunTextWidth(
	const impl::ResolvedTextRun& run, std::u32string_view text, float global_shrink
) {
	PTGN_ASSERT(run.font, "Valid font required");

	float size{ run.style.size * global_shrink };
	float width{ 0.0f };

	for (auto i{ 0uz }; i < text.size(); ++i) {
		std::uint32_t codepoint{ text[i] };

		width += run.font->GetGlyphAdvance(codepoint) * size;

		if (i + 1 < text.size()) {
			std::uint32_t next_codepoint{ text[i + 1] };

			width += (run.font->GetKerning(codepoint, next_codepoint) * run.style.kerning +
					  run.style.tracking) *
					 size;
		}
	}

	return width;
}

float GetInterGlyphSpacing(
	const impl::ResolvedStyledText& styled_text, std::uint32_t left_codepoint,
	std::size_t left_run_index, std::uint32_t right_codepoint, std::size_t right_run_index,
	float global_shrink
) {
	PTGN_ASSERT(left_run_index < styled_text.runs.size());
	PTGN_ASSERT(right_run_index < styled_text.runs.size());

	const auto& left_run{ styled_text.runs[left_run_index] };
	const auto& right_run{ styled_text.runs[right_run_index] };

	PTGN_ASSERT(left_run.font, "Valid left font required");
	PTGN_ASSERT(right_run.font, "Valid right font required");

	float left_size{ left_run.style.size * global_shrink };

	// Tracking belongs after the left glyph, so use the left run's style.
	float spacing{ left_run.style.tracking * left_size };

	// Preserve kerning across boundaries that only change color or another
	// non-metric style. Avoid kerning between different fonts or sizes.
	if (bool compatible_for_kerning{ left_run.font == right_run.font &&
									 NearlyEqual(left_run.style.size, right_run.style.size) };
		compatible_for_kerning) {
		spacing += left_run.font->GetKerning(left_codepoint, right_codepoint) *
				   left_run.style.kerning * left_size;
	}

	return spacing;
}

float GetSpacingBeforeGlyph(const TextLayoutBuildContext& ctx, const ResolvedGlyph& resolved) {
	if (ctx.current_line_glyphs.empty()) {
		return 0.0f;
	}

	const auto& previous{ ctx.current_line_glyphs.back() };

	return GetInterGlyphSpacing(
		ctx.styled_text, previous.codepoint, previous.source_run_index, resolved.codepoint,
		resolved.source_run_index, ctx.global_shrink
	);
}

float GetTokenLeadingSpacing(const TextLayoutBuildContext& ctx, const RichTextToken& token) {
	if (ctx.current_line_glyphs.empty() || token.text.empty()) {
		return 0.0f;
	}

	const auto& previous{ ctx.current_line_glyphs.back() };

	return GetInterGlyphSpacing(
		ctx.styled_text, previous.codepoint, previous.source_run_index, token.text.front(),
		token.run_index, ctx.global_shrink
	);
}

float GetTokenAppendWidth(const TextLayoutBuildContext& ctx, const RichTextToken& token) {
	return GetTokenLeadingSpacing(ctx, token) + token.width;
}

TextBatchStyle GetGlyphBatchStyle(const TextLayout& layout, const Glyph& glyph) {
	PTGN_ASSERT(
		glyph.source_run_index < layout.batch_styles.size(),
		"Glyph source run index does not have a matching text batch style"
	);

	const auto& style{ layout.batch_styles[glyph.source_run_index] };

	PTGN_ASSERT(
		style.texture == glyph.texture,
		"Glyph texture does not match the texture resolved for its source run"
	);

	return style;
}

void ApplyItalicShear(std::array<V2_float, 4>& positions) {
	constexpr float kItalicShear{ -0.25f };

	float min_y{ positions[0].y };
	float max_y{ positions[0].y };

	for (const auto& position : positions) {
		min_y = std::min(min_y, position.y);
		max_y = std::max(max_y, position.y);
	}

	float center_y{ (min_y + max_y) * 0.5f };

	for (auto& position : positions) {
		position.x += (position.y - center_y) * kItalicShear;
	}
}

void AddTextDecorationsForLine(
	const impl::ResolvedStyledText& styled_text, const std::vector<Glyph>& line_glyphs,
	std::size_t line_index, float global_shrink, std::vector<TextDecoration>& decorations
) {
	if (line_glyphs.empty()) {
		return;
	}

	auto begin{ line_glyphs.begin() };

	while (begin != line_glyphs.end()) {
		auto run_index{ begin->source_run_index };

		auto end{ begin };
		while (end != line_glyphs.end() && end->source_run_index == run_index) {
			++end;
		}

		if (run_index >= styled_text.runs.size()) {
			begin = end;
			continue;
		}

		const auto& run{ styled_text.runs[run_index] };
		const auto& style{ run.style };

		bool underline{ HasFlag(style.flags, FontStyle::Underline) };
		bool strikethrough{ HasFlag(style.flags, FontStyle::Strikethrough) };

		if (!underline && !strikethrough) {
			begin = end;
			continue;
		}

		float size{ style.size * global_shrink };
		float thickness{ std::max(1.0f, size * 0.065f) };

		float x_min{ begin->position.x };
		float x_max{ begin->position.x };

		for (auto it{ begin }; it != end; ++it) {
			x_min = std::min(x_min, it->position.x);
			x_max = std::max(x_max, it->position.x + it->advance);
		}

		float baseline_y{ begin->position.y };

		if (underline) {
			float y{ baseline_y + size * 0.12f };

			decorations.emplace_back(
				TextDecoration{
					.type			  = TextDecorationType::Underline,
					.rect			  = Rect{ { x_min, y }, { x_max, y + thickness } },
					.color			  = style.color,
					.source_run_index = run_index,
					.line_index		  = line_index,
					.visible		  = true,
				}
			);
		}

		if (strikethrough) {
			float y{ baseline_y - size * 0.28f };

			decorations.emplace_back(
				TextDecoration{
					.type			  = TextDecorationType::Strikethrough,
					.rect			  = Rect{ { x_min, y }, { x_max, y + thickness } },
					.color			  = style.color,
					.source_run_index = run_index,
					.line_index		  = line_index,
					.visible		  = true,
				}
			);
		}

		begin = end;
	}
}

impl::TextDrawBatch& GetOrCreateTextBatch(
	std::vector<impl::TextDrawBatch>& batches, const TextBatchStyle& style, bool decoration
) {
	if (batches.empty() || batches.back().style != style ||
		batches.back().decoration != decoration) {
		auto& batch{ batches.emplace_back() };
		batch.style		 = style;
		batch.decoration = decoration;
		return batch;
	}

	return batches.back();
}

TextLineMetrics MeasureLineMetrics(const impl::ResolvedTextRun& run, float global_shrink) {
	PTGN_ASSERT(run.font, "Valid font required for text run");

	auto metrics{ run.font->GetMetrics() };

	float size{ run.style.size * global_shrink };

	TextLineMetrics result;
	result.height  = (metrics.line_height + run.style.line_spacing) * size;
	result.ascent  = metrics.ascender * size;
	result.descent = -metrics.descender * size;

	return result;
}

Rect GetGlyphVisualRect(const Glyph& glyph) {
	return Rect{
		glyph.position + glyph.plane.min,
		glyph.position + glyph.plane.max,
	};
}

Rect GetGlyphLogicalRect(const TextLayout& layout, const Glyph& glyph) {
	PTGN_ASSERT(glyph.line_index < layout.lines.size(), "Glyph line index is out of range");

	float left{ glyph.position.x };
	float right{ glyph.position.x + glyph.advance };

	if (right < left) {
		std::swap(left, right);
	}

	const auto& line{ layout.lines[glyph.line_index] };

	return Rect{
		{ left, line.logical_top },
		{ right, line.logical_bottom },
	};
}

Rect GetGlyphClipTestRect(const TextLayout& layout, const Glyph& glyph, TextClipMode mode) {
	if (mode == TextClipMode::Clip) {
		// Character-level clipping should use the logical advance cell.
		// Otherwise negative left bearings make first glyphs disappear.
		return GetGlyphLogicalRect(layout, glyph);
	}

	// Partial clipping should use the actual visual quad.
	return GetGlyphVisualRect(glyph);
}

bool ShouldDrawRectWithClipMode(const Rect& rect, const Rect& clip_rect, TextClipMode mode) {
	switch (mode) {
		using enum TextClipMode;

		case None:		  return true;

		case Clip:		  return impl::RectContainsRect(clip_rect, rect);

		case ClipPartial: return impl::Intersects(rect, clip_rect);
	}

	return true;
}

V2_float GetEffectOffset(const Glyph& glyph, float time) {
	const auto& effect{ glyph.render_style.effect };
	auto order{ static_cast<float>(glyph.visible_order) };

	float phase{ effect.phase + order * kGlyphEffectPhaseStep };
	float t{ time * effect.speed + phase };

	auto oscillate = [&](const GlyphEffectOscillation& oscillation) {
		auto angle = [&](float frequency_multiplier, float glyph_phase_multiplier) {
			return t * effect.frequency * frequency_multiplier + order * glyph_phase_multiplier;
		};

		return V2_float{
			std::sin(
				angle(oscillation.frequency_multiplier.x, oscillation.glyph_phase_multiplier.x)
			) * effect.amplitude,
			std::cos(
				angle(oscillation.frequency_multiplier.y, oscillation.glyph_phase_multiplier.y)
			) * effect.amplitude,
		};
	};

	switch (effect.type) {
		using enum GlyphEffectType;

		case Wobble: return oscillate(kWobbleOscillation);

		case Wave:
			return {
				0.0f,
				std::sin(t * effect.frequency) * effect.amplitude,
			};

		case Shake: return oscillate(kShakeOscillation);

		case Pulse: [[fallthrough]];
		case None:	[[fallthrough]];
		default:	return {};
	}
}

float GetEffectScale(const Glyph& glyph, float time) {
	const auto& effect{ glyph.render_style.effect };

	if (effect.type != GlyphEffectType::Pulse) {
		return 1.0f;
	}

	auto order{ static_cast<float>(glyph.visible_order) };
	float phase{ effect.phase + order * kGlyphEffectPhaseStep };

	return 1.0f + std::sin(time * effect.speed + phase) * effect.amplitude;
}

float MeasureTokenWidth(
	const RichTextToken& token, const impl::ResolvedStyledText& styled_text, float global_shrink
) {
	if (token.run_index >= styled_text.runs.size()) {
		return 0.0f;
	}

	const auto& run{ styled_text.runs[token.run_index] };

	PTGN_ASSERT(run.font, "Valid font required for text run");

	float size{ run.style.size * global_shrink };

	if (token.type == RichTextToken::Type::Tab) {
		return run.font->GetGlyphAdvance(U' ') * size * 4.0f;
	}

	float width{ 0.0f };

	for (auto i{ 0uz }; i < token.text.size(); ++i) {
		std::uint32_t codepoint{ token.text[i] };

		width += run.font->GetGlyphAdvance(codepoint) * size;

		if (i + 1 < token.text.size()) {
			std::uint32_t next_codepoint{ token.text[i + 1] };

			width += (run.font->GetKerning(codepoint, next_codepoint) * run.style.kerning +
					  run.style.tracking) *
					 size;
		}
	}

	return width;
}

std::vector<RichTextToken> Tokenize(
	const impl::ResolvedStyledText& styled_text, bool collapse_spaces, float global_shrink
) {
	std::vector<RichTextToken> tokens;

	bool previous_was_collapsible_space{ true };

	auto emit_space = [&](std::size_t run_index, bool tab, auto i) {
		if (collapse_spaces) {
			if (previous_was_collapsible_space) {
				return;
			}

			RichTextToken token;
			token.type		= RichTextToken::Type::Space;
			token.run_index = run_index;
			token.text.push_back(U' ');
			token.width					 = MeasureTokenWidth(token, styled_text, global_shrink);
			token.source_codepoint_begin = i;
			tokens.push_back(std::move(token));

			previous_was_collapsible_space = true;
			return;
		}

		RichTextToken token;
		token.type		= tab ? RichTextToken::Type::Tab : RichTextToken::Type::Space;
		token.run_index = run_index;
		token.text.push_back(tab ? U'\t' : U' ');
		token.width					 = MeasureTokenWidth(token, styled_text, global_shrink);
		token.source_codepoint_begin = run_index;
		tokens.push_back(std::move(token));

		previous_was_collapsible_space = true;
	};

	for (auto run_index{ 0uz }; run_index < styled_text.runs.size(); ++run_index) {
		const auto& run{ styled_text.runs[run_index] };
		std::u32string decoded{ DecodeUtf8(run.text) };

		auto i{ 0uz };
		while (i < decoded.size()) {
			std::uint32_t cp{ decoded[i] };

			if (cp == U'\r') {
				++i;
				continue;
			}

			if (cp == U'\n') {
				RichTextToken token;
				token.type		= RichTextToken::Type::Newline;
				token.run_index = run_index;
				token.text.push_back(U'\n');
				token.source_codepoint_begin = i;
				tokens.push_back(std::move(token));

				previous_was_collapsible_space = true;

				++i;
				continue;
			}

			if (cp == U' ' || cp == U'\t') {
				if (collapse_spaces) {
					while (i < decoded.size() && (decoded[i] == U' ' || decoded[i] == U'\t')) {
						++i;
					}

					emit_space(run_index, false, i);
					continue;
				}

				if (cp == U' ') {
					std::size_t begin{ i };
					while (i < decoded.size() && decoded[i] == U' ') {
						++i;
					}

					RichTextToken token;
					token.type		= RichTextToken::Type::Space;
					token.run_index = run_index;
					token.text		= decoded.substr(begin, i - begin);
					token.width		= MeasureTokenWidth(token, styled_text, global_shrink);
					token.source_codepoint_begin = i;
					tokens.push_back(std::move(token));

					previous_was_collapsible_space = true;
					continue;
				}

				emit_space(run_index, true, i);
				++i;
				continue;
			}

			std::size_t begin{ i };
			while (i < decoded.size() && !IsWhitespace(decoded[i])) {
				++i;
			}

			RichTextToken token;
			token.type					 = RichTextToken::Type::Word;
			token.run_index				 = run_index;
			token.text					 = decoded.substr(begin, i - begin);
			token.width					 = MeasureTokenWidth(token, styled_text, global_shrink);
			token.source_codepoint_begin = i;
			tokens.push_back(std::move(token));

			previous_was_collapsible_space = false;
		}
	}

	if (collapse_spaces && !tokens.empty() && tokens.back().type == RichTextToken::Type::Space) {
		tokens.pop_back();
	}

	return tokens;
}

std::optional<ResolvedGlyph> ResolveGlyph(
	const impl::ResolvedTextRun& run, std::uint32_t codepoint, std::size_t source_run_index,
	std::size_t source_codepoint_index, float global_shrink
) {
	PTGN_ASSERT(run.font, "Valid font required for text run");

	std::optional<impl::GlyphMetrics> metrics{ run.font->GetGlyph(codepoint) };

	if (!metrics.has_value()) {
		metrics = run.font->GetGlyph(U'?');
	}

	if (!metrics.has_value()) {
		return std::nullopt;
	}

	ResolvedGlyph resolved;
	resolved.codepoint				= codepoint;
	resolved.metrics				= metrics.value();
	resolved.source_run_index		= source_run_index;
	resolved.source_codepoint_index = source_codepoint_index;
	resolved.texture				= run.font->GetTexture();

	resolved.render_style.color			   = run.style.color;
	resolved.render_style.flags			   = run.style.flags;
	resolved.render_style.effect.type	   = run.style.effect.type;
	resolved.render_style.effect.amplitude = run.style.effect.amplitude;
	resolved.render_style.effect.frequency = run.style.effect.frequency;
	resolved.render_style.effect.speed	   = run.style.effect.speed;
	resolved.render_style.effect.phase	   = run.style.effect.phase;

	float size{ run.style.size * global_shrink };

	resolved.metrics.plane.min *= size;
	resolved.metrics.plane.max *= size;

	// Store only the glyph's base advance. Pair spacing is applied when the
	// following glyph is appended.
	resolved.metrics.advance *= size;

	return resolved;
}

bool IsJustificationSpace(const Glyph& glyph) {
	return glyph.codepoint == U' ' || glyph.codepoint == U'\t';
}

template <typename TRun>
void IncludeRunLineMetrics(TextLayoutBuildContext& ctx, const TRun& run) {
	auto metrics{ MeasureLineMetrics(run, ctx.global_shrink) };

	ctx.current_line_size.y = std::max(ctx.current_line_size.y, metrics.height);

	ctx.current_line_ascent = std::max(ctx.current_line_ascent, metrics.ascent);

	ctx.current_line_descent = std::max(ctx.current_line_descent, metrics.descent);
}

Glyph BuildGlyph(const ResolvedGlyph& resolved, V2_float position) {
	Glyph glyph;
	glyph.codepoint				 = resolved.codepoint;
	glyph.position				 = position;
	glyph.plane					 = resolved.metrics.plane;
	glyph.uv					 = resolved.metrics.uv;
	glyph.source_run_index		 = resolved.source_run_index;
	glyph.advance				 = resolved.metrics.advance;
	glyph.source_codepoint_index = resolved.source_codepoint_index;
	glyph.render_style			 = resolved.render_style;
	glyph.texture				 = resolved.texture;
	return glyph;
}

void AppendGlyph(TextLayoutBuildContext& ctx, const ResolvedGlyph& resolved, float layout_advance) {
	ctx.current_line_size.x += GetSpacingBeforeGlyph(ctx, resolved);

	ctx.current_line_glyphs.push_back(BuildGlyph(resolved, { ctx.current_line_size.x, ctx.y }));

	ctx.current_line_size.x += layout_advance;
}

void AppendGlyph(TextLayoutBuildContext& ctx, const ResolvedGlyph& resolved) {
	AppendGlyph(ctx, resolved, resolved.metrics.advance);
}

void FlushLine(TextLayoutBuildContext& ctx, LineFlushReason reason) {
	if (reason == LineFlushReason::SoftWrap) {
		ctx.layout.wrapped = true;
	}

	bool ends_with_explicit_newline{ reason == LineFlushReason::ExplicitNewline };
	bool is_last_line_of_paragraph{ reason != LineFlushReason::SoftWrap };

	if (ctx.current_line_glyphs.empty() && !ends_with_explicit_newline) {
		return;
	}

	LineLayout line;
	line.glyph_begin = ctx.layout.glyphs.size();
	line.glyph_end	 = ctx.layout.glyphs.size() + ctx.current_line_glyphs.size();
	line.size		 = ctx.current_line_size;

	float baseline_y{ ctx.box.rect.min.y + ctx.y + ctx.current_line_ascent };

	line.logical_top = baseline_y - ctx.current_line_ascent;

	line.logical_bottom = baseline_y + ctx.current_line_descent;

	auto justify_space_count{
		std::ranges::count_if(ctx.current_line_glyphs, IsJustificationSpace)
	};

	float x_offset{ 0.0f };
	float justify_extra_per_space{ 0.0f };

	switch (ctx.box.style.horizontal_align) {
		using enum HorizontalAlign;

		case Left: x_offset = ctx.box.rect.min.x; break;

		case Center:
			x_offset = ctx.box.rect.min.x + (ctx.box.rect.GetSize().x - line.size.x) * 0.5f;
			break;

		case Right:	  x_offset = ctx.box.rect.min.x + ctx.box.rect.GetSize().x - line.size.x; break;

		case Justify: {
			x_offset = ctx.box.rect.min.x;

			bool should_justify{ justify_space_count > 0 &&
								 (!is_last_line_of_paragraph || ctx.box.style.justify_last_line) };

			if (float remaining_width{ ctx.box.rect.GetSize().x - line.size.x };
				should_justify && remaining_width > 0.0f) {
				justify_extra_per_space = remaining_width / static_cast<float>(justify_space_count);

				line.size.x += remaining_width;
			}

			break;
		}
	}

	float line_top{ ctx.box.rect.min.y + ctx.y };

	line.bounds = Rect{
		{ x_offset, line_top },
		{ x_offset + line.size.x, line_top + line.size.y },
	};

	float justify_extra{ 0.0f };

	for (Glyph& glyph : ctx.current_line_glyphs) {
		glyph.position.x += x_offset + justify_extra;

		// Every glyph initially has ctx.y as its y position, so either set
		// it directly to the baseline or retain the previous addition.
		glyph.position.y = baseline_y;

		glyph.line_index	= ctx.layout.lines.size();
		glyph.visible_order = ctx.visible_order++;

		if (ctx.box.style.horizontal_align == HorizontalAlign::Justify &&
			justify_extra_per_space > 0.0f && IsJustificationSpace(glyph)) {
			justify_extra += justify_extra_per_space;
		}
	}

	AddTextDecorationsForLine(
		ctx.styled_text, ctx.current_line_glyphs, ctx.layout.lines.size(), ctx.global_shrink,
		ctx.layout.decorations
	);

	ctx.layout.glyphs.append_range(ctx.current_line_glyphs);
	ctx.layout.lines.push_back(line);

	ctx.layout.measured_size.x = std::max(ctx.layout.measured_size.x, line.size.x);

	ctx.layout.measured_size.y += line.size.y;

	ctx.current_line_glyphs.clear();
	ctx.current_line_size	  = {};
	ctx.current_line_ascent	  = 0.0f;
	ctx.current_line_descent  = 0.0f;
	ctx.y					 += line.size.y;
}

bool UsesCharacterWrapping(const TextLayoutBuildContext& ctx, const RichTextToken& token) {
	return token.type == RichTextToken::Type::Word && ctx.can_wrap &&
		   ctx.box.style.wrap_mode == WrapMode::Character;
}

bool ShouldBreakOversizedWord(const TextLayoutBuildContext& ctx, const RichTextToken& token) {
	return token.type == RichTextToken::Type::Word && ctx.can_wrap &&
		   ctx.box.style.wrap_mode == WrapMode::Word &&
		   ctx.box.style.allow_word_break_in_overflow && token.width > ctx.wrap_width;
}

bool ShouldWrapBeforeToken(
	const TextLayoutBuildContext& ctx, const RichTextToken& token, bool character_wrap_word
) {
	return ctx.can_wrap && !character_wrap_word && ctx.current_line_size.x > 0.0f &&
		   ctx.current_line_size.x + GetTokenAppendWidth(ctx, token) > ctx.wrap_width;
}

float MeasureGlyphAppendWidth(
	const TextLayoutBuildContext& ctx, std::span<const ResolvedGlyph> glyphs,
	const ResolvedGlyph* suffix = nullptr
) {
	float width{ 0.0f };

	std::optional<std::uint32_t> previous_codepoint;
	std::optional<std::size_t> previous_run_index;

	if (!ctx.current_line_glyphs.empty()) {
		const auto& previous{ ctx.current_line_glyphs.back() };

		previous_codepoint = previous.codepoint;
		previous_run_index = previous.source_run_index;
	}

	auto measure_glyph = [&](const ResolvedGlyph& glyph) {
		if (previous_codepoint.has_value()) {
			width += GetInterGlyphSpacing(
				ctx.styled_text, previous_codepoint.value(), previous_run_index.value(),
				glyph.codepoint, glyph.source_run_index, ctx.global_shrink
			);
		}

		width += glyph.metrics.advance;

		previous_codepoint = glyph.codepoint;
		previous_run_index = glyph.source_run_index;
	};

	for (const auto& glyph : glyphs) {
		measure_glyph(glyph);
	}

	if (suffix) {
		measure_glyph(*suffix);
	}

	return width;
}

std::size_t GetSpacingCodepointBegin(const RichTextToken& token, bool wrapped_before_token) {
	if (wrapped_before_token && token.type == RichTextToken::Type::Space) {
		return 1;
	}

	return 0;
}

template <typename TRun>
void AppendSpacingToken(
	TextLayoutBuildContext& ctx, const RichTextToken& token, const TRun& run,
	std::size_t codepoint_begin
) {
	if (token.type == RichTextToken::Type::Tab) {
		// TODO: Get rid of this resolve glyph and add custom tab width support.
		if (std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
				run, U'\t', token.run_index, token.source_codepoint_begin, ctx.global_shrink
			) };
			resolved.has_value()) {
			AppendGlyph(ctx, resolved.value(), token.width);
		} else {
			ctx.current_line_size.x += token.width;
		}

		return;
	}

	PTGN_ASSERT(token.type == RichTextToken::Type::Space);
	PTGN_ASSERT(codepoint_begin <= token.text.size());

	for (auto i{ codepoint_begin }; i < token.text.size(); ++i) {
		std::uint32_t codepoint{ token.text[i] };

		std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
			run, codepoint, token.run_index, token.source_codepoint_begin + i, ctx.global_shrink
		) };

		if (resolved.has_value()) {
			AppendGlyph(ctx, resolved.value());
		}
	}
}

template <typename TRun>
std::vector<ResolvedGlyph> ResolveTokenGlyphs(
	const TextLayoutBuildContext& ctx, const RichTextToken& token, const TRun& run
) {
	std::vector<ResolvedGlyph> glyphs;
	glyphs.reserve(token.text.size());

	for (auto i{ 0uz }; i < token.text.size(); ++i) {
		std::uint32_t codepoint{ token.text[i] };

		std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
			run, codepoint, token.run_index, token.source_codepoint_begin + i, ctx.global_shrink
		) };

		if (resolved.has_value()) {
			glyphs.push_back(std::move(resolved.value()));
		}
	}

	return glyphs;
}

template <typename TRun>
void AppendCharacterWrappedWord(
	TextLayoutBuildContext& ctx, const RichTextToken& token, const TRun& run
) {
	std::vector<ResolvedGlyph> word_glyphs{ ResolveTokenGlyphs(ctx, token, run) };

	std::optional<ResolvedGlyph> hyphen_glyph;

	if (ctx.box.style.insert_hyphen_on_split) {
		hyphen_glyph = ResolveGlyph(
			run, U'-', token.run_index, token.source_codepoint_begin, ctx.global_shrink
		);
	}

	auto word_begin{ 0uz };

	while (word_begin < word_glyphs.size()) {
		std::size_t remaining_count{ word_glyphs.size() - word_begin };

		auto remaining_glyphs{ std::span{ word_glyphs }.subspan(word_begin) };

		if (float remaining_word_width{ MeasureGlyphAppendWidth(ctx, remaining_glyphs) };
			ctx.current_line_size.x + remaining_word_width <= ctx.wrap_width) {
			for (auto i{ word_begin }; i < word_glyphs.size(); ++i) {
				AppendGlyph(ctx, word_glyphs[i]);
			}

			break;
		}

		bool add_hyphen{ ctx.box.style.insert_hyphen_on_split && hyphen_glyph.has_value() };

		std::size_t fit_count{ 0 };

		for (auto count{ 1uz }; count < remaining_count; ++count) {
			auto prefix{ std::span{ word_glyphs }.subspan(word_begin, count) };

			const ResolvedGlyph* suffix{ add_hyphen ? &hyphen_glyph.value() : nullptr };

			float candidate_width{ ctx.current_line_size.x +
								   MeasureGlyphAppendWidth(ctx, prefix, suffix) };

			if (candidate_width <= ctx.wrap_width) {
				fit_count = count;
			}
		}

		std::size_t remainder_count{ remaining_count - fit_count };

		if (bool invalid_split{
				fit_count == 0 || (ctx.box.style.prevent_single_letter_split && fit_count == 1) ||
				(ctx.box.style.require_three_letter_remainder && remainder_count < 3) };
			invalid_split) {
			if (!ctx.current_line_glyphs.empty()) {
				FlushLine(ctx, LineFlushReason::SoftWrap);
				IncludeRunLineMetrics(ctx, run);
				continue;
			}

			// It cannot be split validly on an empty line, so preserve the
			// word and allow it to overflow.
			for (auto i{ word_begin }; i < word_glyphs.size(); ++i) {
				AppendGlyph(ctx, word_glyphs[i]);
			}

			break;
		}

		for (auto offset{ 0uz }; offset < fit_count; ++offset) {
			AppendGlyph(ctx, word_glyphs[word_begin + offset]);
		}

		if (add_hyphen) {
			ResolvedGlyph inserted_hyphen{ hyphen_glyph.value() };

			inserted_hyphen.source_codepoint_index =
				word_glyphs[word_begin + fit_count - 1].source_codepoint_index;

			AppendGlyph(ctx, inserted_hyphen);
		}

		word_begin += fit_count;

		FlushLine(ctx, LineFlushReason::SoftWrap);
		IncludeRunLineMetrics(ctx, run);
	}
}

template <typename TRun>
void AppendBrokenOversizedWord(
	TextLayoutBuildContext& ctx, const RichTextToken& token, const TRun& run
) {
	for (auto i{ 0uz }; i < token.text.size(); ++i) {
		std::uint32_t codepoint{ token.text[i] };

		std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
			run, codepoint, token.run_index, token.source_codepoint_begin + i, ctx.global_shrink
		) };

		if (!resolved.has_value()) {
			continue;
		}

		auto resolved_span{ std::span<const ResolvedGlyph>{ &resolved.value(), 1 } };

		if (float append_width{ MeasureGlyphAppendWidth(ctx, resolved_span) };
			ctx.current_line_size.x > 0.0f &&
			ctx.current_line_size.x + append_width > ctx.wrap_width) {
			FlushLine(ctx, LineFlushReason::SoftWrap);
			IncludeRunLineMetrics(ctx, run);
		}

		AppendGlyph(ctx, resolved.value());
	}
}

template <typename TRun>
void AppendUnbrokenToken(TextLayoutBuildContext& ctx, const RichTextToken& token, const TRun& run) {
	for (auto i{ 0uz }; i < token.text.size(); ++i) {
		std::uint32_t codepoint{ token.text[i] };

		std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
			run, codepoint, token.run_index, token.source_codepoint_begin + i, ctx.global_shrink
		) };

		if (resolved.has_value()) {
			AppendGlyph(ctx, resolved.value());
		}
	}
}

void ProcessToken(TextLayoutBuildContext& ctx, const RichTextToken& token) {
	if (token.type == RichTextToken::Type::Newline) {
		const auto& run{ ctx.styled_text.runs[token.run_index] };
		IncludeRunLineMetrics(ctx, run);
		FlushLine(ctx, LineFlushReason::ExplicitNewline);
		return;
	}

	bool character_wrap_word{ UsesCharacterWrapping(ctx, token) };
	bool break_oversized_word{ ShouldBreakOversizedWord(ctx, token) };

	bool wrapped_before_token{ ShouldWrapBeforeToken(ctx, token, character_wrap_word) };

	if (wrapped_before_token) {
		FlushLine(ctx, LineFlushReason::SoftWrap);
	}

	std::size_t spacing_codepoint_begin{ GetSpacingCodepointBegin(token, wrapped_before_token) };

	// A single wrapped space is removed completely. With multiple spaces,
	// only the first is removed and the remaining spaces start the new line.
	if (token.type == RichTextToken::Type::Space && spacing_codepoint_begin == token.text.size()) {
		return;
	}

	const auto& run{ ctx.styled_text.runs[token.run_index] };

	IncludeRunLineMetrics(ctx, run);

	if (token.IsSpacing()) {
		AppendSpacingToken(ctx, token, run, spacing_codepoint_begin);
		return;
	}

	if (character_wrap_word) {
		AppendCharacterWrappedWord(ctx, token, run);
		return;
	}

	if (break_oversized_word) {
		AppendBrokenOversizedWord(ctx, token, run);
		return;
	}

	AppendUnbrokenToken(ctx, token, run);
}

TextLayout BuildLayoutAtScale(
	const impl::ResolvedStyledText& styled_text, const TextBox& box, float global_shrink
) {
	std::vector<RichTextToken> tokens{
		Tokenize(styled_text, box.style.collapse_spaces, global_shrink)
	};

	TextLayoutBuildContext ctx{ styled_text, box, global_shrink };

	for (const auto& token : tokens) {
		ProcessToken(ctx, token);
	}

	FlushLine(ctx, LineFlushReason::EndOfText);

	return std::move(ctx.layout);
}

float FindBestShrinkScale(const impl::ResolvedStyledText& styled_text, const TextBox& box) {
	float lo{ box.style.shrink_scale.min };
	float hi{ box.style.shrink_scale.max };
	float best{ lo };

	// Binary search for best scale.
	for (int i{ 0 }; i < kShrinkScaleSearchIterations; ++i) {
		float mid{ 0.5f * (lo + hi) };
		auto layout{ BuildLayoutAtScale(styled_text, box, mid) };
		if (impl::TextLayoutFitsInBox(layout, box.rect)) {
			best = mid;
			lo	 = mid;
		} else {
			hi = mid;
		}
	}

	return best;
}

void ApplyVerticalAlignment(const TextBox& box, TextLayout& layout) {
	if (!box.rect.GetSize().IsPositive()) {
		return;
	}

	if (!layout.bounds.GetSize().IsPositive()) {
		return;
	}

	Rect bounds{ layout.bounds };

	float offset_y{ 0.0f };

	switch (box.style.vertical_align) {
		using enum VerticalAlign;

		case Top:	 offset_y = box.rect.min.y - bounds.min.y; break;

		case Center: {
			float box_center_y{ (box.rect.min.y + box.rect.max.y) * 0.5f };

			float content_center_y{ (bounds.min.y + bounds.max.y) * 0.5f };

			offset_y = box_center_y - content_center_y;
			break;
		}

		case Bottom: offset_y = box.rect.max.y - bounds.max.y; break;
	}

	for (Glyph& glyph : layout.glyphs) {
		glyph.position.y += offset_y;
	}

	for (TextDecoration& decoration : layout.decorations) {
		decoration.rect.min.y += offset_y;
		decoration.rect.max.y += offset_y;
	}

	for (LineLayout& line : layout.lines) {
		line.logical_top	+= offset_y;
		line.logical_bottom += offset_y;

		line.bounds.min.y += offset_y;
		line.bounds.max.y += offset_y;
	}

	layout.bounds.min.y += offset_y;
	layout.bounds.max.y += offset_y;
}

void ApplyMaxLines(const TextBox& box, TextLayout& layout) {
	if (box.style.max_lines == 0 || layout.lines.size() <= box.style.max_lines) {
		return;
	}

	auto keep_lines{ box.style.max_lines };
	auto last_line_index{ keep_lines - 1 };
	auto hide_from{ layout.lines[last_line_index].glyph_end };

	layout.glyphs.erase(
		layout.glyphs.begin() + static_cast<std::ptrdiff_t>(hide_from), layout.glyphs.end()
	);

	for (auto i{ hide_from }; i < layout.glyphs.size(); ++i) {
		layout.glyphs[i].visible = false;
	}

	for (auto& decoration : layout.decorations) {
		if (decoration.line_index >= keep_lines) {
			decoration.visible = false;
		}
	}

	layout.lines.resize(keep_lines);
	layout.truncated_by_max_lines = true;

	layout.measured_size.y = 0.0f;
	for (const auto& line : layout.lines) {
		layout.measured_size.y += line.size.y;
	}
}

void ApplyEllipsisOverflow(
	const impl::ResolvedStyledText& styled_text, const TextBox& box, float global_shrink,
	TextLayout& layout
) {
	if (layout.lines.empty()) {
		return;
	}

	if (!box.rect.GetSize().IsPositive()) {
		ApplyMaxLines(box, layout);
		return;
	}

	auto keep_lines{ layout.lines.size() };

	if (box.style.max_lines > 0) {
		keep_lines = std::min(keep_lines, box.style.max_lines);
	}

	while (keep_lines > 0) {
		float line_bottom{ 0.0f };
		for (auto i{ 0uz }; i < keep_lines; ++i) {
			line_bottom += layout.lines[i].size.y;
		}

		if (line_bottom <= box.rect.GetSize().y || keep_lines == 1) {
			break;
		}

		--keep_lines;
	}

	if (keep_lines == 0) {
		for (auto& glyph : layout.glyphs) {
			glyph.visible = false;
		}
		layout.lines.clear();
		layout.ellipsized			  = true;
		layout.truncated_by_max_lines = true;
		layout.measured_size		  = {};
		return;
	}

	auto last_visible_line_index{ keep_lines - 1 };
	const auto& last_line{ layout.lines[last_visible_line_index] };

	bool line_count_truncated{ keep_lines < layout.lines.size() };

	bool width_overflow{ false };
	for (auto i{ last_line.glyph_begin }; i < last_line.glyph_end; ++i) {
		if (i >= layout.glyphs.size()) {
			continue;
		}

		const auto& glyph{ layout.glyphs[i] };
		if (!glyph.visible) {
			continue;
		}

		float right{ glyph.position.x + glyph.plane.max.x };
		if (right > box.rect.max.x) {
			width_overflow = true;
			break;
		}
	}

	if (!line_count_truncated && !width_overflow) {
		return;
	}

	auto hide_from{ last_line.glyph_end };
	for (auto i{ hide_from }; i < layout.glyphs.size(); ++i) {
		layout.glyphs[i].visible = false;
	}

	const impl::ResolvedTextRun* source_run{ nullptr };
	std::size_t source_run_index{ 0 };

	if (last_line.glyph_begin < layout.glyphs.size()) {
		const auto& anchor{ layout.glyphs[last_line.glyph_begin] };
		if (anchor.source_run_index < styled_text.runs.size()) {
			source_run		 = &styled_text.runs[anchor.source_run_index];
			source_run_index = anchor.source_run_index;
		}
	}

	if (!source_run) {
		layout.lines.resize(keep_lines);
		layout.ellipsized			  = true;
		layout.truncated_by_max_lines = line_count_truncated;
		return;
	}

	PTGN_ASSERT(source_run, "Source run not found");

	std::u32string dots{ U"..." };

	float dots_width{ MeasureRunTextWidth(*source_run, dots, global_shrink) };

	float usable_right{ box.rect.max.x - dots_width };

	auto cutoff{ last_line.glyph_end };

	for (auto i{ last_line.glyph_begin }; i < last_line.glyph_end; ++i) {
		if (i >= layout.glyphs.size()) {
			continue;
		}

		const auto& glyph{ layout.glyphs[i] };
		if (!glyph.visible) {
			continue;
		}

		float right{ glyph.position.x + glyph.plane.max.x };
		if (right > usable_right) {
			cutoff = i;
			break;
		}
	}

	for (auto i{ cutoff }; i < last_line.glyph_end; ++i) {
		if (i < layout.glyphs.size()) {
			layout.glyphs[i].visible = false;
		}
	}

	float start_x{ box.rect.min.x };

	if (cutoff > last_line.glyph_begin) {
		const auto& previous{ layout.glyphs[cutoff - 1] };

		start_x = previous.position.x + previous.advance;

		start_x += GetInterGlyphSpacing(
			styled_text, previous.codepoint, previous.source_run_index, U'.', source_run_index,
			global_shrink
		);
	}

	float y{ 0.0f };
	if (last_line.glyph_begin < layout.glyphs.size()) {
		y = layout.glyphs[last_line.glyph_begin].position.y;
	} else if (!layout.glyphs.empty()) {
		y = layout.glyphs.back().position.y;
	}

	layout.glyphs.erase(
		layout.glyphs.begin() + static_cast<std::ptrdiff_t>(cutoff), layout.glyphs.end()
	);

	for (auto i{ 0uz }; i < dots.size(); ++i) {
		auto codepoint{ static_cast<std::uint32_t>(dots[i]) };

		if (i > 0) {
			start_x += GetInterGlyphSpacing(
				styled_text, static_cast<std::uint32_t>(dots[i - 1]), source_run_index, codepoint,
				source_run_index, global_shrink
			);
		}

		auto resolved{ ResolveGlyph(*source_run, codepoint, source_run_index, i, global_shrink) };

		if (!resolved.has_value()) {
			continue;
		}

		Glyph glyph{ BuildGlyph(resolved.value(), { start_x, y }) };
		glyph.line_index	= last_visible_line_index;
		glyph.visible_order = layout.glyphs.size();

		layout.glyphs.push_back(glyph);

		start_x += resolved->metrics.advance;
	}

	for (auto& decoration : layout.decorations) {
		if (decoration.line_index >= keep_lines) {
			decoration.visible = false;
		}
	}

	layout.lines.resize(keep_lines);
	layout.ellipsized			  = true;
	layout.truncated_by_max_lines = line_count_truncated;

	auto& updated_last_line{ layout.lines.back() };
	updated_last_line.glyph_end = layout.glyphs.size();

	layout.measured_size.y = 0.0f;
	for (const auto& line : layout.lines) {
		layout.measured_size.y += line.size.y;
	}
}

void EmitDecorationQuad(
	const TextDecoration& decoration, Color tint, Depth depth, int entity_id,
	std::vector<impl::TextureQuad>& quads
) {
	std::array positions{
		decoration.rect.min,
		V2_float{ decoration.rect.max.x, decoration.rect.min.y },
		decoration.rect.max,
		V2_float{ decoration.rect.min.x, decoration.rect.max.y },
	};

	std::array tex_coords{ impl::GetDefaultTextureCoordinates<false>() };

	auto color{ Color::Multiply(decoration.color, tint) };

	auto color_n{ color.Normalized() };

	quads.emplace_back(impl::CreateTextureQuad(positions, depth, color_n, tex_coords, entity_id));
}

void EmitGlyphQuad(
	const Glyph& glyph, Color tint, Depth depth, int entity_id, float time,
	std::vector<impl::TextureQuad>& quads
) {
	auto effect_offset{ GetEffectOffset(glyph, time) };

	auto quad_min{ glyph.position + glyph.plane.min + effect_offset };
	auto quad_max{ glyph.position + glyph.plane.max + effect_offset };

	if (float scale{ GetEffectScale(glyph, time) }; !NearlyEqual(scale, 1.0f)) {
		auto center{ (quad_min + quad_max) * 0.5f };
		quad_min = center + (quad_min - center) * scale;
		quad_max = center + (quad_max - center) * scale;
	}

	std::array positions{ quad_min, V2_float{ quad_max.x, quad_min.y }, quad_max,
						  V2_float{ quad_min.x, quad_max.y } };

	if (HasFlag(glyph.render_style.flags, FontStyle::Italic)) {
		ApplyItalicShear(positions);
	}

	std::array tex_coords{ glyph.uv.min, V2_float{ glyph.uv.max.x, glyph.uv.min.y }, glyph.uv.max,
						   V2_float{ glyph.uv.min.x, glyph.uv.max.y } };

	auto color{ Color::Multiply(glyph.render_style.color, tint) };

	auto color_n{ color.Normalized() };

	quads.emplace_back(impl::CreateTextureQuad(positions, depth, color_n, tex_coords, entity_id));
}

} // namespace

namespace impl {

TextLayout BuildTextLayout(const ResolvedStyledText& styled_text, const TextBox& box) {
	float shrink{ 1.0f };

	if (box.style.overflow_mode == OverflowMode::ScaleToFit) {
		shrink = FindBestShrinkScale(styled_text, box);
	}

	auto layout{ BuildLayoutAtScale(styled_text, box, shrink) };

	RecalculateLayoutGeometry(layout);

	bool exceeds_max_lines{ box.style.max_lines > 0 && layout.lines.size() > box.style.max_lines };

	layout.fits = !box.rect.GetSize().IsPositive() ||
				  (!exceeds_max_lines && TextLayoutFitsInBox(layout, box.rect));

	RecalculateLayoutGeometry(layout);

	if (box.style.overflow_mode == OverflowMode::Ellipsis) {
		ApplyEllipsisOverflow(styled_text, box, shrink, layout);
	} else {
		ApplyMaxLines(box, layout);
	}

	RecalculateLayoutGeometry(layout);

	ApplyVerticalAlignment(box, layout);

	RecalculateLayoutGeometry(layout);

	layout.clip_rect = std::nullopt;
	layout.clip_mode = TextClipMode::None;

	if (box.rect.GetSize().IsPositive()) {
		switch (box.style.overflow_mode) {
			using enum OverflowMode;

			case Clip:
				layout.clip_rect	 = box.rect;
				layout.clip_mode	 = TextClipMode::Clip;
				layout.uses_clipping = true;
				break;

			case ClipPartial:
				layout.clip_rect	 = box.rect;
				layout.clip_mode	 = TextClipMode::ClipPartial;
				layout.uses_clipping = true;
				break;

			case Overflow:	 [[fallthrough]];
			case Ellipsis:	 [[fallthrough]];
			case ScaleToFit: break;
		}
	}

	if (box.rect.GetSize().IsPositive()) {
		layout.local_box = box.rect;
	} else {
		layout.local_box = layout.bounds;
	}

	ReindexVisibleGlyphs(layout);

	return layout;
}

TextMeasurement MeasureText(const ResolvedStyledText& styled_text, const TextBox& box) {
	TextLayout layout{ BuildTextLayout(styled_text, box) };

	TextMeasurement result;
	result.size				 = layout.measured_size;
	result.line_count		 = layout.lines.size();
	result.used_shrink_scale = layout.used_shrink_scale;
	result.truncated		 = layout.ellipsized || layout.truncated_by_max_lines;

	if (!layout.lines.empty()) {
		result.first_line_height = layout.lines.front().size.y;
	}

	return result;
}

std::vector<UniformWrite> GetTextUniforms(const DistanceFieldStyle& sdf, bool is_decoration) {
	PTGN_ASSERT(sdf.pixel_range > 0.0f, "Invalid font pixel range");

	return {
		{ "u_Weight", sdf.weight },
		{ "u_Softness", sdf.softness },

		{ "u_OutlineColor", sdf.outline_color.Normalized() },
		{ "u_OutlineWidth", sdf.outline_width },
		{ "u_OutlineSoftness", sdf.outline_softness },

		{ "u_ShadowColor", sdf.shadow_color.Normalized() },
		{ "u_ShadowOffset", sdf.shadow_offset },
		{ "u_ShadowWidth", sdf.shadow_width },
		{ "u_ShadowSoftness", sdf.shadow_softness },

		{ "u_OuterGlowColor", sdf.outer_glow_color.Normalized() },
		{ "u_OuterGlowWidth", sdf.outer_glow_width },
		{ "u_OuterGlowSoftness", sdf.outer_glow_softness },

		{ "u_InnerGlowColor", sdf.inner_glow_color.Normalized() },
		{ "u_InnerGlowWidth", sdf.inner_glow_width },
		{ "u_InnerGlowSoftness", sdf.inner_glow_softness },

		{ "u_PixelRange", sdf.pixel_range },
		{ "u_IsDecoration", is_decoration ? 1.0f : 0.0f },
	};
}

std::vector<TextDrawBatch> BuildTextDrawBatches(const DrawTextRequest& request) {
	std::vector<TextDrawBatch> batches;

	for (const Glyph& glyph : request.layout.glyphs) {
		if (!glyph.visible) {
			continue;
		}
		if (glyph.visible_order >= request.reveal_glyph_count) {
			continue;
		}

		if (request.clip_rect.has_value()) {
			PTGN_ASSERT(
				request.clip_rect.value().GetSize().IsPositive(),
				"If clip rect is set its size must be positive"
			);

			auto glyph_rect{ GetGlyphClipTestRect(request.layout, glyph, request.clip_mode) };

			if (!ShouldDrawRectWithClipMode(
					glyph_rect, request.clip_rect.value(), request.clip_mode
				)) {
				continue;
			}
		}

		if (auto batch_style{ GetGlyphBatchStyle(request.layout, glyph) };
			batches.empty() || batches.back().style != batch_style) {
			auto& batch{ batches.emplace_back() };
			batch.style = batch_style;
		}

		EmitGlyphQuad(
			glyph, request.tint, request.depth, request.entity_id, request.time,
			batches.back().quads
		);
	}

	for (const auto& decoration : request.layout.decorations) {
		if (!decoration.visible) {
			continue;
		}

		if (request.clip_rect.has_value()) {
			PTGN_ASSERT(
				request.clip_rect.value().GetSize().IsPositive(),
				"If clip rect is set its size must be positive"
			);

			if (!ShouldDrawRectWithClipMode(
					decoration.rect, request.clip_rect.value(), request.clip_mode
				)) {
				continue;
			}
		}

		PTGN_ASSERT(
			decoration.source_run_index < request.layout.batch_styles.size(),
			"Decoration source run index does not have a matching text batch style"
		);

		const auto& batch_style{ request.layout.batch_styles[decoration.source_run_index] };
		auto& batch{ GetOrCreateTextBatch(batches, batch_style, true) };

		EmitDecorationQuad(decoration, request.tint, request.depth, request.entity_id, batch.quads);
	}

	return batches;
}

bool TextLayoutFitsInBox(const TextLayout& layout, Rect box) {
	return layout.measured_size.x <= box.GetSize().x && layout.measured_size.y <= box.GetSize().y;
}

} // namespace impl

} // namespace ptgn
