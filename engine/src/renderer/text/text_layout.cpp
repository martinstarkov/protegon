#include "renderer/text/text_layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
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

struct RichTextToken {
	enum class Type : std::uint8_t {
		Word,
		Space,
		Tab,
		Newline,
	};

	Type type{ Type::Word };
	std::u32string text;
	std::size_t run_index{ 0 };
	float width{ 0.0f };
};

struct ResolvedGlyph {
	std::uint32_t codepoint{ 0 };
	impl::GlyphMetrics metrics;
	GlyphRenderStyle render_style;
	std::size_t source_run_index{ 0 };
	std::size_t source_codepoint_index{ 0 };
	impl::TextureId texture{ 0 };
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

std::uint32_t GetNextCodepoint(std::u32string_view text, std::size_t index) {
	if (index + 1 < text.size()) {
		return text[index + 1];
	}
	return 0;
}

std::optional<Rect> GetVisibleGlyphBounds(const TextLayout& layout) {
	bool found{ false };
	V2_float min;
	V2_float max;

	for (const auto& glyph : layout.glyphs) {
		if (!glyph.visible) {
			continue;
		}

		V2_float glyph_min{ glyph.position + glyph.plane.min };
		V2_float glyph_max{ glyph.position + glyph.plane.max };

		if (!found) {
			min	  = glyph_min;
			max	  = glyph_max;
			found = true;
		} else {
			min.x = std::min(min.x, glyph_min.x);
			min.y = std::min(min.y, glyph_min.y);
			max.x = std::max(max.x, glyph_max.x);
			max.y = std::max(max.y, glyph_max.y);
		}
	}

	if (!found) {
		return std::nullopt;
	}

	return Rect{ min, max };
}

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

struct TextLineMetrics {
	float height{ 0.0f };
	float ascent{ 0.0f };
	float descent{ 0.0f };
};

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

Rect GetGlyphLogicalRect(const Glyph& glyph) {
	float left{ glyph.position.x };
	float right{ glyph.position.x + glyph.advance };

	if (right < left) {
		std::swap(left, right);
	}

	float top{ glyph.position.y + glyph.plane.min.y };
	float bottom{ glyph.position.y + glyph.plane.max.y };

	return Rect{
		{ left, top },
		{ right, bottom },
	};
}

Rect GetGlyphClipTestRect(const Glyph& glyph, TextClipMode mode) {
	if (mode == TextClipMode::Clip) {
		// Character-level clipping should use the logical advance cell.
		// Otherwise negative left bearings make first glyphs disappear.
		return GetGlyphLogicalRect(glyph);
	}

	// Partial clipping should use the actual visual quad.
	return GetGlyphVisualRect(glyph);
}

bool RectFullyContains(Rect outer, Rect inner) {
	return inner.min.x >= outer.min.x && inner.max.x <= outer.max.x && inner.min.y >= outer.min.y &&
		   inner.max.y <= outer.max.y;
}

bool RectIntersects(Rect a, Rect b) {
	return a.max.x > b.min.x && a.min.x < b.max.x && a.max.y > b.min.y && a.min.y < b.max.y;
}

bool ShouldDrawRectWithClipMode(const Rect& rect, const Rect& clip_rect, TextClipMode mode) {
	switch (mode) {
		using enum TextClipMode;

		case None:		  return true;

		case Clip:		  return RectFullyContains(clip_rect, rect);

		case ClipPartial: return RectIntersects(rect, clip_rect);
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
		float space_adv{ run.font->GetAdvance(U' ', 0) };
		return (space_adv + run.style.kerning + run.style.tracking) * size * 4.0f;
	}

	float width{ 0.0f };
	for (auto i{ 0uz }; i < token.text.size(); ++i) {
		std::uint32_t cp{ token.text[i] };
		std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
		width +=
			(run.font->GetAdvance(cp, next_cp) + run.style.kerning + run.style.tracking) * size;
	}

	return width;
}

std::vector<RichTextToken> Tokenize(
	const impl::ResolvedStyledText& styled_text, bool collapse_spaces, float global_shrink
) {
	std::vector<RichTextToken> tokens;

	bool previous_was_collapsible_space{ true };

	auto emit_space = [&](std::size_t run_index, bool tab) {
		if (collapse_spaces) {
			if (previous_was_collapsible_space) {
				return;
			}

			RichTextToken token;
			token.type		= RichTextToken::Type::Space;
			token.run_index = run_index;
			token.text.push_back(U' ');
			token.width = MeasureTokenWidth(token, styled_text, global_shrink);
			tokens.push_back(std::move(token));

			previous_was_collapsible_space = true;
			return;
		}

		RichTextToken token;
		token.type		= tab ? RichTextToken::Type::Tab : RichTextToken::Type::Space;
		token.run_index = run_index;
		token.text.push_back(tab ? U'\t' : U' ');
		token.width = MeasureTokenWidth(token, styled_text, global_shrink);
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

					emit_space(run_index, false);
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
					tokens.push_back(std::move(token));

					previous_was_collapsible_space = true;
					continue;
				}

				emit_space(run_index, true);
				++i;
				continue;
			}

			std::size_t begin{ i };
			while (i < decoded.size() && !IsWhitespace(decoded[i])) {
				++i;
			}

			RichTextToken token;
			token.type		= RichTextToken::Type::Word;
			token.run_index = run_index;
			token.text		= decoded.substr(begin, i - begin);
			token.width		= MeasureTokenWidth(token, styled_text, global_shrink);
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
	const impl::ResolvedTextRun& run, std::uint32_t codepoint, std::uint32_t next_codepoint,
	std::size_t source_run_index, std::size_t source_codepoint_index, float global_shrink
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

	resolved.metrics.plane.min *= run.style.size * global_shrink;
	resolved.metrics.plane.max *= run.style.size * global_shrink;
	resolved.metrics.advance =
		(run.font->GetAdvance(codepoint, next_codepoint) + run.style.kerning + run.style.tracking) *
		(run.style.size * global_shrink);

	return resolved;
}

TextLayout BuildLayoutAtScale(
	const impl::ResolvedStyledText& styled_text, const TextBox& box, float global_shrink
) {
	std::vector<RichTextToken> tokens{
		Tokenize(styled_text, box.style.collapse_spaces, global_shrink)
	};

	TextLayout layout;
	layout.used_shrink_scale = global_shrink;
	layout.batch_styles		 = BuildBatchStyles(styled_text);

	std::vector<Glyph> current_line_glyphs;
	V2_float current_line_size;
	float current_line_ascent{ 0.0f };
	float current_line_descent{ 0.0f };
	float y{ 0.0f };
	std::size_t visible_order{ 0 };

	enum class LineFlushReason {
		SoftWrap,
		ExplicitNewline,
		EndOfText
	};

	auto flush_line = [&](LineFlushReason reason) {
		bool ends_with_explicit_newline{ reason == LineFlushReason::ExplicitNewline };
		bool is_last_line_of_paragraph{ reason != LineFlushReason::SoftWrap };

		if (current_line_glyphs.empty() && !ends_with_explicit_newline) {
			return;
		}

		LineLayout line;
		line.glyph_begin = layout.glyphs.size();
		line.glyph_end	 = layout.glyphs.size() + current_line_glyphs.size();
		line.size		 = current_line_size;

		// auto line_baseline_y{ y + current_line_ascent }; // NOSONAR

		std::size_t justify_space_count{ 0 };

		for (const Glyph& glyph : current_line_glyphs) {
			if (glyph.codepoint == U' ' || glyph.codepoint == U'\t') {
				++justify_space_count;
			}
		}

		float x_offset{ 0.0f };
		float justify_extra_per_space{ 0.0f };

		switch (box.style.horizontal_align) {
			using enum HorizontalAlign;

			case Left: x_offset = box.rect.min.x; break;

			case Center:
				x_offset = box.rect.min.x + (box.rect.GetSize().x - line.size.x) * 0.5f;
				break;

			case Right:	  x_offset = box.rect.min.x + (box.rect.GetSize().x - line.size.x); break;

			case Justify: {
				x_offset = box.rect.min.x;

				bool should_justify{ justify_space_count > 0 &&
									 (!is_last_line_of_paragraph || box.style.justify_last_line) };

				if (float remaining_width{ box.rect.GetSize().x - line.size.x };
					should_justify && remaining_width > 0.0f) {
					justify_extra_per_space =
						remaining_width / static_cast<float>(justify_space_count);
				}

				break;
			}
		}

		float justify_extra{ 0.0f };

		for (Glyph& glyph : current_line_glyphs) {
			glyph.position.x	+= x_offset + justify_extra;
			glyph.position.y	+= box.rect.min.y + current_line_ascent;
			glyph.line_index	 = layout.lines.size();
			glyph.visible_order	 = visible_order++;

			if (box.style.horizontal_align == HorizontalAlign::Justify &&
				justify_extra_per_space > 0.0f &&
				(glyph.codepoint == U' ' || glyph.codepoint == U'\t')) {
				justify_extra += justify_extra_per_space;
			}
		}

		AddTextDecorationsForLine(
			styled_text, current_line_glyphs, layout.lines.size(), global_shrink, layout.decorations
		);

		layout.glyphs.append_range(current_line_glyphs);
		layout.lines.push_back(line);

		layout.measured_size.x	= std::max(layout.measured_size.x, line.size.x);
		layout.measured_size.y += line.size.y;

		current_line_glyphs.clear();
		current_line_size	  = {};
		current_line_ascent	  = 0.0f;
		current_line_descent  = 0.0f;
		y					 += line.size.y;
	};

	for (RichTextToken& token : tokens) {
		if (token.type == RichTextToken::Type::Newline) {
			flush_line(LineFlushReason::ExplicitNewline);
			continue;
		}

		float wrap_width{ box.rect.GetSize().x };
		bool can_wrap{ box.style.wrap_mode != WrapMode::None && wrap_width > 0.0f };

		bool character_wrap_word{ token.type == RichTextToken::Type::Word && can_wrap &&
								  box.style.wrap_mode == WrapMode::Character };

		bool break_oversized_word{ token.type == RichTextToken::Type::Word && can_wrap &&
								   box.style.wrap_mode == WrapMode::Word &&
								   box.style.allow_word_break_in_overflow &&
								   token.width > wrap_width };

		if (bool wrap_here{ can_wrap && current_line_size.x > 0.0f &&
							current_line_size.x + token.width > wrap_width };
			wrap_here && !character_wrap_word) {
			flush_line(LineFlushReason::SoftWrap);
		}

		const auto& run{ styled_text.runs[token.run_index] };

		auto line_metrics{ MeasureLineMetrics(run, global_shrink) };

		auto restore_line_metrics = [&]() {
			current_line_size.y	 = std::max(current_line_size.y, line_metrics.height);
			current_line_ascent	 = std::max(current_line_ascent, line_metrics.ascent);
			current_line_descent = std::max(current_line_descent, line_metrics.descent);
		};

		restore_line_metrics();

		if (token.type == RichTextToken::Type::Space || token.type == RichTextToken::Type::Tab) {
			if (std::optional<ResolvedGlyph> space_glyph{ ResolveGlyph(
					run, token.type == RichTextToken::Type::Space ? U' ' : U'\t', 0,
					token.run_index, 0, global_shrink
				) };
				space_glyph.has_value()) {
				Glyph glyph;
				glyph.codepoint				 = space_glyph.value().codepoint;
				glyph.position				 = { current_line_size.x, y };
				glyph.plane					 = space_glyph.value().metrics.plane;
				glyph.uv					 = space_glyph.value().metrics.uv;
				glyph.source_run_index		 = token.run_index;
				glyph.advance				 = space_glyph.value().metrics.advance;
				glyph.source_codepoint_index = 0;
				glyph.render_style			 = space_glyph.value().render_style;
				glyph.texture				 = space_glyph.value().texture;
				current_line_glyphs.push_back(glyph);
			}

			current_line_size.x += token.width;
			continue;
		}

		auto append_glyph = [&](const ResolvedGlyph& resolved) {
			Glyph glyph;
			glyph.codepoint				 = resolved.codepoint;
			glyph.position				 = { current_line_size.x, y };
			glyph.plane					 = resolved.metrics.plane;
			glyph.uv					 = resolved.metrics.uv;
			glyph.source_run_index		 = resolved.source_run_index;
			glyph.advance				 = resolved.metrics.advance;
			glyph.source_codepoint_index = resolved.source_codepoint_index;
			glyph.render_style			 = resolved.render_style;
			glyph.texture				 = resolved.texture;
			current_line_glyphs.push_back(glyph);

			current_line_size.x += resolved.metrics.advance;
		};

		if (character_wrap_word) {
			std::vector<ResolvedGlyph> word_glyphs;
			word_glyphs.reserve(token.text.size());

			for (auto i{ 0uz }; i < token.text.size(); ++i) {
				std::uint32_t cp{ token.text[i] };
				std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };

				std::optional<ResolvedGlyph> resolved{
					ResolveGlyph(run, cp, next_cp, token.run_index, i, global_shrink)
				};

				if (resolved.has_value()) {
					word_glyphs.push_back(std::move(resolved.value()));
				}
			}

			std::optional<ResolvedGlyph> hyphen_glyph;

			if (box.style.insert_hyphen_on_split) {
				hyphen_glyph = ResolveGlyph(run, U'-', 0, token.run_index, 0, global_shrink);
			}

			auto word_begin{ 0uz };

			while (word_begin < word_glyphs.size()) {
				std::size_t remaining_count{ word_glyphs.size() - word_begin };

				float remaining_word_width{ 0.0f };

				for (auto i{ word_begin }; i < word_glyphs.size(); ++i) {
					remaining_word_width += word_glyphs[i].metrics.advance;
				}

				if (current_line_size.x + remaining_word_width <= wrap_width) {
					for (auto i{ word_begin }; i < word_glyphs.size(); ++i) {
						append_glyph(word_glyphs[i]);
					}

					break;
				}

				bool add_hyphen{ box.style.insert_hyphen_on_split && hyphen_glyph.has_value() };

				std::size_t fit_count{ 0 };
				std::optional<ResolvedGlyph> fitted_last_glyph;

				float prefix_width{ 0.0f };

				// Only test actual splits. A count equal to remaining_count
				// would mean the complete suffix fits, which was tested above.
				for (auto count{ 1uz }; count < remaining_count; ++count) {
					auto glyph_index{ word_begin + count - 1 };

					prefix_width += word_glyphs[glyph_index].metrics.advance;

					std::uint32_t boundary_next_cp{ add_hyphen ? U'-' : 0 };

					std::optional<ResolvedGlyph> boundary_glyph{ ResolveGlyph(
						run, word_glyphs[glyph_index].codepoint, boundary_next_cp, token.run_index,
						word_glyphs[glyph_index].source_codepoint_index, global_shrink
					) };

					if (!boundary_glyph.has_value()) {
						continue;
					}

					// Replace the original advance, which included kerning
					// against the next source character, with the advance at
					// the actual line boundary.
					float candidate_width{ current_line_size.x + prefix_width -
										   word_glyphs[glyph_index].metrics.advance +
										   boundary_glyph.value().metrics.advance };

					if (add_hyphen) {
						candidate_width += hyphen_glyph.value().metrics.advance;
					}

					if (candidate_width <= wrap_width) {
						fit_count		  = count;
						fitted_last_glyph = std::move(boundary_glyph.value());
					}
				}

				std::size_t remainder_count{ remaining_count - fit_count };

				bool invalid_split{
					fit_count == 0 || (box.style.prevent_single_letter_split && fit_count == 1) ||
					(box.style.require_three_letter_remainder && remainder_count < 3)
				};

				if (invalid_split) {
					// The split available on this line violates one of the
					// enabled rules. Move the complete remaining word to a
					// fresh line and try again.
					if (!current_line_glyphs.empty()) {
						flush_line(LineFlushReason::SoftWrap);
						restore_line_metrics();
						continue;
					}

					// The word is already on an empty line and still has no
					// valid split. Keep it intact and allow horizontal
					// overflow rather than violating the requested rules.
					for (auto i{ word_begin }; i < word_glyphs.size(); ++i) {
						append_glyph(word_glyphs[i]);
					}

					break;
				}

				for (auto offset{ 0uz }; offset < fit_count; ++offset) {
					bool is_last{ offset + 1 == fit_count };

					if (is_last) {
						append_glyph(fitted_last_glyph.value());
					} else {
						append_glyph(word_glyphs[word_begin + offset]);
					}
				}

				if (add_hyphen) {
					ResolvedGlyph inserted_hyphen{ hyphen_glyph.value() };

					// The hyphen is synthetic, so associate it with the
					// preceding source character.
					inserted_hyphen.source_codepoint_index =
						word_glyphs[word_begin + fit_count - 1].source_codepoint_index;

					append_glyph(inserted_hyphen);
				}

				word_begin += fit_count;

				flush_line(LineFlushReason::SoftWrap);
				restore_line_metrics();
			}

			continue;
		}

		if (break_oversized_word) {
			for (auto i{ 0uz }; i < token.text.size(); ++i) {
				std::uint32_t cp{ token.text[i] };
				std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };

				std::optional<ResolvedGlyph> resolved{
					ResolveGlyph(run, cp, next_cp, token.run_index, i, global_shrink)
				};

				if (!resolved.has_value()) {
					continue;
				}

				if (float advance{ resolved.value().metrics.advance };
					current_line_size.x > 0.0f && current_line_size.x + advance > wrap_width) {
					flush_line(LineFlushReason::SoftWrap);
					restore_line_metrics();
				}

				append_glyph(resolved.value());
			}

			continue;
		}

		float x{ current_line_size.x };

		for (auto i{ 0uz }; i < token.text.size(); ++i) {
			std::uint32_t cp{ token.text[i] };
			std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };

			std::optional<ResolvedGlyph> resolved{
				ResolveGlyph(run, cp, next_cp, token.run_index, i, global_shrink)
			};

			if (!resolved.has_value()) {
				continue;
			}

			Glyph glyph;
			glyph.codepoint				 = resolved.value().codepoint;
			glyph.position				 = { x, y };
			glyph.plane					 = resolved.value().metrics.plane;
			glyph.uv					 = resolved.value().metrics.uv;
			glyph.source_run_index		 = resolved.value().source_run_index;
			glyph.advance				 = resolved.value().metrics.advance;
			glyph.source_codepoint_index = resolved.value().source_codepoint_index;
			glyph.render_style			 = resolved.value().render_style;
			glyph.texture				 = resolved.value().texture;
			current_line_glyphs.push_back(glyph);

			x += resolved.value().metrics.advance;
		}

		current_line_size.x = x;
	}

	flush_line(LineFlushReason::EndOfText);

	return layout;
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

	auto bounds{ GetVisibleGlyphBounds(layout) };
	if (!bounds.has_value()) {
		return;
	}

	float offset_y{ 0.0f };

	switch (box.style.vertical_align) {
		using enum VerticalAlign;

		case Top:	 offset_y = box.rect.min.y - bounds.value().min.y; break;

		case Center: {
			float box_center_y{ (box.rect.min.y + box.rect.max.y) * 0.5f };
			float content_center_y{ (bounds.value().min.y + bounds.value().max.y) * 0.5f };
			offset_y = box_center_y - content_center_y;
			break;
		}

		case Bottom: offset_y = box.rect.max.y - bounds.value().max.y; break;
	}

	for (Glyph& glyph : layout.glyphs) {
		glyph.position.y += offset_y;
	}

	for (TextDecoration& decoration : layout.decorations) {
		decoration.rect.min.y += offset_y;
		decoration.rect.max.y += offset_y;
	}

	layout.content_offset.y += offset_y;
}

void ApplyMaxLines(const TextBox& box, TextLayout& layout) {
	if (box.style.max_lines == 0 || layout.lines.size() <= box.style.max_lines) {
		return;
	}

	auto keep_lines{ box.style.max_lines };
	auto last_line_index{ keep_lines - 1 };
	auto hide_from{ layout.lines[last_line_index].glyph_end };

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
	PTGN_ASSERT(source_run->font, "Source run font must be valid");

	const auto& font{ *source_run->font };

	std::u32string dots{ U"..." };

	float dots_width{ 0.0f };
	for (auto i{ 0uz }; i < dots.size(); ++i) {
		auto cp{ static_cast<std::uint32_t>(dots[i]) };
		auto next_cp{ GetNextCodepoint(dots, i) };

		dots_width += (font.GetAdvance(cp, next_cp) + source_run->style.kerning +
					   source_run->style.tracking) *
					  (source_run->style.size * global_shrink);
	}

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
		const auto& prev{ layout.glyphs[cutoff - 1] };
		start_x = prev.position.x + prev.advance;
	}

	float y{ 0.0f };
	if (last_line.glyph_begin < layout.glyphs.size()) {
		y = layout.glyphs[last_line.glyph_begin].position.y;
	} else if (!layout.glyphs.empty()) {
		y = layout.glyphs.back().position.y;
	}

	for (auto i{ 0uz }; i < dots.size(); ++i) {
		auto cp{ static_cast<std::uint32_t>(dots[i]) };
		auto next_cp{ GetNextCodepoint(dots, i) };

		std::optional<ResolvedGlyph> resolved{
			ResolveGlyph(*source_run, cp, next_cp, source_run_index, i, global_shrink)
		};

		if (!resolved.has_value()) {
			continue;
		}

		Glyph glyph;
		glyph.codepoint				 = resolved.value().codepoint;
		glyph.position				 = { start_x, y };
		glyph.plane					 = resolved.value().metrics.plane;
		glyph.uv					 = resolved.value().metrics.uv;
		glyph.source_run_index		 = resolved.value().source_run_index;
		glyph.advance				 = resolved.value().metrics.advance;
		glyph.source_codepoint_index = resolved.value().source_codepoint_index;
		glyph.render_style			 = resolved.value().render_style;
		glyph.line_index			 = last_visible_line_index;
		glyph.visible_order			 = layout.glyphs.size();
		glyph.texture				 = resolved.value().texture;

		layout.glyphs.push_back(glyph);

		start_x += resolved.value().metrics.advance;
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

void ApplyClipVisibility(Rect clip_rect, TextLayout& layout) {
	for (auto& glyph : layout.glyphs) {
		V2_float gmin{ glyph.position + glyph.plane.min };
		V2_float gmax{ glyph.position + glyph.plane.max };

		if (gmax.x <= clip_rect.min.x || gmin.x >= clip_rect.max.x || gmax.y <= clip_rect.min.y ||
			gmin.y >= clip_rect.max.y) {
			glyph.visible = false;
		}
	}

	for (auto& decoration : layout.decorations) {
		if (decoration.rect.max.x <= clip_rect.min.x || decoration.rect.min.x >= clip_rect.max.x ||
			decoration.rect.max.y <= clip_rect.min.y || decoration.rect.min.y >= clip_rect.max.y) {
			decoration.visible = false;
		}
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

	if (box.style.overflow_mode == OverflowMode::Ellipsis) {
		ApplyEllipsisOverflow(styled_text, box, shrink, layout);
	} else {
		ApplyMaxLines(box, layout);
	}

	ApplyVerticalAlignment(box, layout);

	layout.clip_rect = std::nullopt;
	layout.clip_mode = TextClipMode::None;

	if (box.rect.GetSize().IsPositive()) {
		switch (box.style.overflow_mode) {
			using enum OverflowMode;

			case Clip:
				layout.clip_rect = box.rect;
				layout.clip_mode = TextClipMode::Clip;
				layout.clipped	 = true;
				break;

			case ClipPartial:
				layout.clip_rect = box.rect;
				layout.clip_mode = TextClipMode::ClipPartial;
				layout.clipped	 = true;
				break;

			case Overflow:	 [[fallthrough]];
			case Ellipsis:	 [[fallthrough]];
			case ScaleToFit: break;
		}
	}

	if (box.rect.GetSize().IsPositive()) {
		layout.local_box = box.rect;
	} else if (auto visible_bounds{ GetVisibleGlyphBounds(layout) }; visible_bounds.has_value()) {
		layout.local_box = visible_bounds.value();
	} else {
		layout.local_box = {};
	}

	return layout;
}

TextMeasurement MeasureText(const ResolvedStyledText& styled_text, const TextBox& box) {
	TextLayout layout{ BuildTextLayout(styled_text, box) };

	TextMeasurement result;
	result.size				 = layout.measured_size;
	result.line_count		 = layout.lines.size();
	result.max_line_width	 = layout.measured_size.x;
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

			auto glyph_rect{ GetGlyphClipTestRect(glyph, request.clip_mode) };

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
