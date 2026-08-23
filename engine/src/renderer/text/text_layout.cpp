#include "renderer/text/text_layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/intersect.h"
#include "core/math/overlap.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
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
constexpr float kGlyphEffectPhaseStep{ 0.35f };

struct GlyphEffectOscillation {
	V2_float frequency_multiplier{ 1.0f, 1.0f };
	V2_float glyph_phase_multiplier{};
};

constexpr GlyphEffectOscillation kWobbleOscillation{
	.frequency_multiplier{ 1.0f, 1.37f },
};

constexpr GlyphEffectOscillation kShakeOscillation{
	.frequency_multiplier{ 17.0f, 23.0f },
	.glyph_phase_multiplier{ 12.9898f, 78.233f },
};

struct SourceCharacter {
	std::uint32_t codepoint{ 0 };
	std::size_t run_index{ 0 };
	std::size_t source_codepoint_index{ 0 };
};

enum class TokenType : std::uint8_t {
	Word,
	Whitespace,
	Newline,
};

struct TextToken {
	TokenType type{ TokenType::Word };
	std::size_t begin{ 0 };
	std::size_t end{ 0 };
};

DistanceFieldStyle ResolveDistanceFieldStyle(
	const impl::FontAtlas& font, const TextRunStyle& style
) {
	auto sdf{ style.sdf };

	sdf.pixel_range = font.GetMetrics().pixel_range;

	if (HasFontFlag(style.flags, FontStyle::Bold)) {
		sdf.weight -= style.bold_weight;
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

	std::u32string decoded;
	decoded.reserve(text.size());

	auto i{ 0uz };
	while (i < text.size()) {
		unsigned char c{ static_cast<unsigned char>(text[i]) };

		if ((c & kAsciiMask) == 0u) {
			decoded.push_back(static_cast<char32_t>(c));
			++i;
			continue;
		}

		if ((c & kTwoByteMask) == kTwoByteLead && i + 1 < text.size()) {
			decoded.push_back(
				static_cast<char32_t>(
					((c & kTwoBytePayloadMask) << kShift6) |
					(static_cast<unsigned char>(text[i + 1]) & kContinuationPayloadMask)
				)
			);
			i += 2;
			continue;
		}

		if ((c & kThreeByteMask) == kThreeByteLead && i + 2 < text.size()) {
			decoded.push_back(
				static_cast<char32_t>(
					((c & kThreeBytePayloadMask) << kShift12) |
					((static_cast<unsigned char>(text[i + 1]) & kContinuationPayloadMask)
					 << kShift6) |
					(static_cast<unsigned char>(text[i + 2]) & kContinuationPayloadMask)
				)
			);
			i += 3;
			continue;
		}

		if ((c & kFourByteMask) == kFourByteLead && i + 3 < text.size()) {
			decoded.push_back(
				static_cast<char32_t>(
					((c & kFourBytePayloadMask) << kShift18) |
					((static_cast<unsigned char>(text[i + 1]) & kContinuationPayloadMask)
					 << kShift12) |
					((static_cast<unsigned char>(text[i + 2]) & kContinuationPayloadMask)
					 << kShift6) |
					(static_cast<unsigned char>(text[i + 3]) & kContinuationPayloadMask)
				)
			);
			i += 4;
			continue;
		}

		decoded.push_back(U'?');
		++i;
	}

	return decoded;
}

bool IsOrdinaryWhitespace(std::uint32_t codepoint) {
	return codepoint == U' ' || codepoint == U'\t';
}

std::vector<SourceCharacter> BuildSourceCharacters(
	const impl::ResolvedStyledText& styled_text, bool collapse_spaces
) {
	std::vector<SourceCharacter> characters;
	std::optional<SourceCharacter> pending_space;
	bool at_line_start{ true };

	for (auto run_index{ 0uz }; run_index < styled_text.runs.size(); ++run_index) {
		const auto& run{ styled_text.runs[run_index] };
		auto decoded{ DecodeUtf8(run.text) };

		for (auto codepoint_index{ 0uz }; codepoint_index < decoded.size(); ++codepoint_index) {
			auto codepoint{ static_cast<std::uint32_t>(decoded[codepoint_index]) };

			if (codepoint == U'\r') {
				continue;
			}

			SourceCharacter character{
				.codepoint				= codepoint,
				.run_index				= run_index,
				.source_codepoint_index = codepoint_index,
			};

			if (!collapse_spaces) {
				characters.push_back(character);
				at_line_start = codepoint == U'\n';
				continue;
			}

			if (codepoint == U'\n') {
				pending_space.reset();
				characters.push_back(character);
				at_line_start = true;
				continue;
			}

			if (IsOrdinaryWhitespace(codepoint)) {
				if (!at_line_start && !pending_space.has_value()) {
					character.codepoint = U' ';
					pending_space		= character;
				}
				continue;
			}

			if (pending_space.has_value()) {
				characters.push_back(pending_space.value());
				pending_space.reset();
			}

			characters.push_back(character);
			at_line_start = false;
		}
	}

	// A pending space is intentionally discarded so collapsing also trims trailing whitespace.
	return characters;
}

std::vector<TextToken> Tokenize(const std::vector<SourceCharacter>& characters) {
	std::vector<TextToken> tokens;

	auto begin{ 0uz };
	while (begin < characters.size()) {
		auto codepoint{ characters[begin].codepoint };

		if (codepoint == U'\n') {
			tokens.emplace_back(
				TextToken{
					.type  = TokenType::Newline,
					.begin = begin,
					.end   = begin + 1,
				}
			);
			++begin;
			continue;
		}

		TokenType type{ IsOrdinaryWhitespace(codepoint) ? TokenType::Whitespace : TokenType::Word };

		auto end{ begin + 1 };
		while (end < characters.size()) {
			if (auto next{ characters[end].codepoint };
				next == U'\n' || IsOrdinaryWhitespace(next) != (type == TokenType::Whitespace)) {
				break;
			}
			++end;
		}

		tokens.emplace_back(
			TextToken{
				.type  = type,
				.begin = begin,
				.end   = end,
			}
		);
		begin = end;
	}

	return tokens;
}

float GetPairSpacing(
	const impl::ResolvedStyledText& styled_text, const SourceCharacter& left,
	const SourceCharacter& right, float scale
) {
	PTGN_ASSERT(left.run_index < styled_text.runs.size());
	PTGN_ASSERT(right.run_index < styled_text.runs.size());

	const auto& left_run{ styled_text.runs[left.run_index] };
	const auto& right_run{ styled_text.runs[right.run_index] };

	PTGN_ASSERT(left_run.font, "Valid left font required");
	PTGN_ASSERT(right_run.font, "Valid right font required");

	float left_size{ left_run.style.size * scale };
	float spacing{ left_run.style.tracking * left_size };

	if (bool can_kern{ left.codepoint != U'\t' && right.codepoint != U'\t' &&
					   left_run.font == right_run.font &&
					   NearlyEqual(left_run.style.size, right_run.style.size) };
		can_kern) {
		spacing += left_run.font->GetKerning(left.codepoint, right.codepoint) *
				   left_run.style.kerning * left_size;
	}

	return spacing;
}

std::optional<impl::GlyphMetrics> GetGlyphMetrics(
	const impl::ResolvedTextRun& run, std::uint32_t codepoint
) {
	PTGN_ASSERT(run.font, "Valid font required for text run");

	auto metrics{ run.font->GetGlyph(codepoint) };
	if (!metrics.has_value()) {
		metrics = run.font->GetGlyph(U'?');
	}
	return metrics;
}

float GetTabAdvance(
	const impl::ResolvedTextRun& run, float cursor_x, std::size_t tab_width, float scale
) {
	PTGN_ASSERT(run.font, "Valid font required for text run");

	float size{ run.style.size * scale };
	float space_advance{ run.font->GetGlyphAdvance(U' ') * size };
	float tab_stop{ space_advance * static_cast<float>(std::max(1uz, tab_width)) };

	if (tab_stop <= 0.0f) {
		return 0.0f;
	}

	float remainder{ std::fmod(std::max(cursor_x, 0.0f), tab_stop) };
	if (NearlyEqual(remainder, 0.0f)) {
		return tab_stop;
	}
	return tab_stop - remainder;
}

float GetCharacterAdvance(
	const impl::ResolvedStyledText& styled_text, const SourceCharacter& character, float cursor_x,
	std::size_t tab_width, float scale
) {
	PTGN_ASSERT(character.run_index < styled_text.runs.size());
	const auto& run{ styled_text.runs[character.run_index] };

	if (character.codepoint == U'\t') {
		return GetTabAdvance(run, cursor_x, tab_width, scale);
	}

	auto metrics{ GetGlyphMetrics(run, character.codepoint) };
	if (!metrics.has_value()) {
		return 0.0f;
	}

	return metrics->advance * run.style.size * scale;
}

GlyphRenderStyle GetGlyphRenderStyle(const TextRunStyle& style) {
	return {
		.color	= style.color,
		.effect = style.effect,
		.flags	= style.flags,
	};
}

struct LayoutBuilder {
	LayoutBuilder(
		const impl::ResolvedStyledText& styled, const TextBox& text_box,
		const std::vector<SourceCharacter>& text_characters, float text_scale
	) :
		styled_text{ styled },
		box{ text_box },
		characters{ text_characters },
		scale{ text_scale },
		can_wrap{ box.HasWidth() && box.style.wrap.mode != WrapMode::None },
		wrap_width{ box.rect.GetSize().x } {
		layout.used_shrink_scale  = scale;
		layout.batch_styles		  = BuildBatchStyles(styled);
		layout.source_glyph_count = static_cast<std::size_t>(
			std::ranges::count_if(characters, [](const SourceCharacter& character) {
				return character.codepoint != U'\n';
			})
		);
	}

	const impl::ResolvedStyledText& styled_text;
	const TextBox& box;
	const std::vector<SourceCharacter>& characters;
	float scale{ 1.0f };
	bool can_wrap{ false };
	float wrap_width{ 0.0f };

	TextLayout layout{};
	LineLayout line{};
	float line_ascent{ 0.0f };
	float line_descent{ 0.0f };
	float line_height{ 0.0f };
	float y{ 0.0f };
	std::optional<SourceCharacter> previous{};

	bool HasLineContent() const {
		return !line.glyphs.empty();
	}

	void IncludeLineMetrics(std::size_t run_index) {
		PTGN_ASSERT(run_index < styled_text.runs.size());
		const auto& run{ styled_text.runs[run_index] };
		PTGN_ASSERT(run.font, "Valid font required for text run");

		auto metrics{ run.font->GetMetrics() };
		float size{ run.style.size * scale };
		float ascent{ metrics.ascender * size };
		float descent{ -metrics.descender * size };
		float height{ (metrics.line_height + run.style.line_spacing) * size };

		line_ascent	 = std::max(line_ascent, ascent);
		line_descent = std::max(line_descent, descent);
		line_height	 = std::max(line_height, std::max(height, ascent + descent));
	}

	float MeasureRange(
		std::size_t begin, std::size_t end, float cursor_x,
		std::optional<SourceCharacter>& previous_character
	) const {
		for (auto i{ begin }; i < end; ++i) {
			const auto& character{ characters[i] };

			if (previous_character.has_value()) {
				cursor_x +=
					GetPairSpacing(styled_text, previous_character.value(), character, scale);
			}

			cursor_x +=
				GetCharacterAdvance(styled_text, character, cursor_x, box.style.tab_width, scale);
			previous_character = character;
		}

		return cursor_x;
	}

	bool Fits(std::size_t begin, std::size_t end) const {
		if (!can_wrap) {
			return true;
		}

		auto measured_previous{ previous };
		float width{ MeasureRange(begin, end, line.size.x, measured_previous) };
		return width <= wrap_width || NearlyEqual(width, wrap_width);
	}

	bool Fits(
		std::size_t first_begin, std::size_t first_end, std::size_t second_begin,
		std::size_t second_end
	) const {
		if (!can_wrap) {
			return true;
		}

		auto measured_previous{ previous };
		float width{ MeasureRange(first_begin, first_end, line.size.x, measured_previous) };
		width = MeasureRange(second_begin, second_end, width, measured_previous);
		return width <= wrap_width || NearlyEqual(width, wrap_width);
	}

	void AppendCharacter(const SourceCharacter& character) {
		PTGN_ASSERT(character.run_index < styled_text.runs.size());
		const auto& run{ styled_text.runs[character.run_index] };
		PTGN_ASSERT(run.font, "Valid font required for text run");

		IncludeLineMetrics(character.run_index);

		if (previous.has_value()) {
			line.size.x += GetPairSpacing(styled_text, previous.value(), character, scale);
		}

		float advance{
			GetCharacterAdvance(styled_text, character, line.size.x, box.style.tab_width, scale)
		};

		Glyph glyph;
		glyph.codepoint				 = character.codepoint;
		glyph.position				 = { line.size.x, 0.0f };
		glyph.texture				 = run.font->GetTexture();
		glyph.source_run_index		 = character.run_index;
		glyph.source_codepoint_index = character.source_codepoint_index;
		glyph.render_style			 = GetGlyphRenderStyle(run.style);
		glyph.advance				 = advance;

		if (character.codepoint != U'\t') {
			if (auto metrics{ GetGlyphMetrics(run, character.codepoint) }) {
				float size{ run.style.size * scale };
				glyph.plane		 = metrics->plane;
				glyph.plane.min *= size;
				glyph.plane.max *= size;
				glyph.uv		 = metrics->uv;
			}
		}

		line.glyphs.push_back(glyph);
		line.size.x += advance;
		previous	 = character;
	}

	void AppendRange(std::size_t begin, std::size_t end) {
		for (auto i{ begin }; i < end; ++i) {
			AppendCharacter(characters[i]);
		}
	}

	void FlushLine(bool paragraph_end, bool wrapped) {
		if (!HasLineContent() && line_height <= 0.0f) {
			return;
		}

		line.size.y		   = line_height;
		line.baseline	   = y + line_ascent;
		line.paragraph_end = paragraph_end;
		line.bounds		   = Rect{ { 0.0f, y }, { line.size.x, y + line.size.y } };

		for (auto& glyph : line.glyphs) {
			glyph.position.y = line.baseline;
		}

		layout.lines.push_back(std::move(line));
		layout.wrapped = layout.wrapped || wrapped;

		y			 += line_height;
		line		  = {};
		line_ascent	  = 0.0f;
		line_descent  = 0.0f;
		line_height	  = 0.0f;
		previous.reset();
	}

	void AppendWhitespace(std::size_t begin, std::size_t end, bool drop_separator_space) {
		if (drop_separator_space && begin < end && characters[begin].codepoint == U' ') {
			++begin;
		}

		for (auto i{ begin }; i < end; ++i) {
			if (can_wrap && !Fits(i, i + 1) && HasLineContent()) {
				// Only the one separator space above is discarded. Any additional user-provided
				// whitespace remains real content even when it wraps onto another line.
				FlushLine(false, true);
			}
			AppendCharacter(characters[i]);
		}
	}

	std::size_t FindCharacterSplit(std::size_t begin, std::size_t end, bool insert_hyphen) const {
		std::size_t fitting_count{ 0 };

		for (auto count{ 1uz }; begin + count <= end; ++count) {
			auto measured_previous{ previous };
			float width{ MeasureRange(begin, begin + count, line.size.x, measured_previous) };

			if (insert_hyphen && begin + count < end) {
				SourceCharacter hyphen{
					.codepoint				= U'-',
					.run_index				= characters[begin + count - 1].run_index,
					.source_codepoint_index = std::numeric_limits<std::size_t>::max(),
				};

				if (measured_previous.has_value()) {
					width += GetPairSpacing(styled_text, measured_previous.value(), hyphen, scale);
				}
				width +=
					GetCharacterAdvance(styled_text, hyphen, width, box.style.tab_width, scale);
			}

			if (width > wrap_width && !NearlyEqual(width, wrap_width)) {
				break;
			}
			fitting_count = count;
		}

		return fitting_count;
	}

	void AppendSyntheticHyphen(const SourceCharacter& anchor) {
		SourceCharacter hyphen{
			.codepoint				= U'-',
			.run_index				= anchor.run_index,
			.source_codepoint_index = std::numeric_limits<std::size_t>::max(),
		};
		AppendCharacter(hyphen);
	}

	void AppendCharacterWrappedWord(std::size_t begin, std::size_t end, bool use_character_rules) {
		while (begin < end) {
			if (!can_wrap || Fits(begin, end)) {
				AppendRange(begin, end);
				return;
			}

			bool insert_hyphen{ use_character_rules && box.style.wrap.insert_hyphen_on_split };
			auto split_count{ FindCharacterSplit(begin, end, insert_hyphen) };

			if (split_count == 0) {
				if (HasLineContent()) {
					FlushLine(false, true);
					continue;
				}

				// A single character wider than the box must be allowed to overflow so this loop
				// always makes progress.
				split_count = 1;
			}

			auto remaining_count{ end - (begin + split_count) };

			if (use_character_rules && HasLineContent() &&
				box.style.wrap.prevent_single_letter_split && split_count == 1) {
				FlushLine(false, true);
				continue;
			}

			if (use_character_rules && box.style.wrap.require_three_letter_remainder &&
				remaining_count > 0 && remaining_count < 3) {
				if (HasLineContent()) {
					FlushLine(false, true);
					continue;
				}

				auto reduce_by{ 3uz - remaining_count };
				if (split_count > reduce_by) {
					split_count		-= reduce_by;
					remaining_count	 = end - (begin + split_count);
				}
			}

			AppendRange(begin, begin + split_count);

			if (bool split_word{ begin + split_count < end }; insert_hyphen && split_word) {
				AppendSyntheticHyphen(characters[begin + split_count - 1]);
			}

			begin += split_count;

			if (begin < end) {
				FlushLine(false, true);
			}
		}
	}

	void AppendWord(const TextToken& word, const std::optional<TextToken>& whitespace) {
		bool has_whitespace{ whitespace.has_value() };

		if (bool fits{ has_whitespace
						   ? Fits(whitespace->begin, whitespace->end, word.begin, word.end)
						   : Fits(word.begin, word.end) };
			!can_wrap || fits) {
			if (has_whitespace) {
				AppendRange(whitespace->begin, whitespace->end);
			}

			AppendRange(word.begin, word.end);
			return;
		}

		if (box.style.wrap.mode == WrapMode::Character) {
			if (has_whitespace) {
				bool wrap_before_separator{ HasLineContent() &&
											whitespace->begin < whitespace->end &&
											characters[whitespace->begin].codepoint == U' ' &&
											!Fits(whitespace->begin, whitespace->begin + 1) };

				if (wrap_before_separator) {
					FlushLine(false, true);
				}

				// If the normal separator space itself caused the wrap, discard
				// exactly that one space. Additional authored spaces are retained.
				AppendWhitespace(whitespace->begin, whitespace->end, wrap_before_separator);
			}

			// Do not flush first. Use the remaining width on the current line.
			AppendCharacterWrappedWord(word.begin, word.end, true);
			return;
		}

		PTGN_ASSERT(box.style.wrap.mode == WrapMode::Word);

		bool wrapped_before_word{ false };

		if (HasLineContent()) {
			FlushLine(false, true);
			wrapped_before_word = true;
		}

		if (has_whitespace) {
			AppendWhitespace(whitespace->begin, whitespace->end, wrapped_before_word);
		}

		if (Fits(word.begin, word.end) || !box.style.wrap.allow_word_break_in_overflow) {
			AppendRange(word.begin, word.end);
			return;
		}

		// Word wrapping only breaks a word when the word cannot fit even on
		// an otherwise empty line.
		AppendCharacterWrappedWord(word.begin, word.end, false);
	}

	TextLayout Build(const std::vector<TextToken>& tokens) {
		std::optional<TextToken> pending_whitespace;

		for (const auto& token : tokens) {
			switch (token.type) {
				case TokenType::Whitespace: pending_whitespace = token; break;

				case TokenType::Newline:	{
					if (pending_whitespace.has_value()) {
						AppendWhitespace(pending_whitespace->begin, pending_whitespace->end, false);
						pending_whitespace.reset();
					}

					IncludeLineMetrics(characters[token.begin].run_index);
					FlushLine(true, false);
					break;
				}

				case TokenType::Word:
					AppendWord(token, pending_whitespace);
					pending_whitespace.reset();
					break;
			}
		}

		if (pending_whitespace.has_value()) {
			AppendWhitespace(pending_whitespace->begin, pending_whitespace->end, false);
		}

		if (HasLineContent()) {
			FlushLine(true, false);
		}

		return std::move(layout);
	}
};

void RecalculateLayoutSize(TextLayout& layout) {
	layout.size = {};

	for (const auto& line : layout.lines) {
		layout.size.x  = std::max(layout.size.x, line.size.x);
		layout.size.y += line.size.y;
	}
}

bool FitsUnclippedLayout(const TextLayout& layout, const TextBox& box) {
	if (box.style.max_lines > 0 && layout.lines.size() > box.style.max_lines) {
		return false;
	}

	auto box_size{ box.rect.GetSize() };
	bool fits_width{ !box.HasWidth() || layout.size.x <= box_size.x ||
					 NearlyEqual(layout.size.x, box_size.x) };
	bool fits_height{ !box.HasHeight() || layout.size.y <= box_size.y ||
					  NearlyEqual(layout.size.y, box_size.y) };
	return fits_width && fits_height;
}

TextLayout BuildLinesAtScale(
	const impl::ResolvedStyledText& styled_text, const TextBox& box,
	const std::vector<SourceCharacter>& characters, const std::vector<TextToken>& tokens,
	float scale
) {
	LayoutBuilder builder{ styled_text, box, characters, scale };
	auto layout{ builder.Build(tokens) };
	RecalculateLayoutSize(layout);
	return layout;
}

float FindBestScale(
	const impl::ResolvedStyledText& styled_text, const TextBox& box,
	const std::vector<SourceCharacter>& characters, const std::vector<TextToken>& tokens
) {
	float min_scale{ box.style.shrink_scale.min };
	float max_scale{ box.style.shrink_scale.max };

	if (!box.HasBox()) {
		return max_scale;
	}

	if (auto maximum_layout{ BuildLinesAtScale(styled_text, box, characters, tokens, max_scale) };
		FitsUnclippedLayout(maximum_layout, box)) {
		return max_scale;
	}

	float low{ min_scale };
	float high{ max_scale };

	for (int i{ 0 }; i < kShrinkScaleSearchIterations; ++i) {
		float middle{ (low + high) * 0.5f };
		auto layout{ BuildLinesAtScale(styled_text, box, characters, tokens, middle) };

		if (FitsUnclippedLayout(layout, box)) {
			low = middle;
		} else {
			high = middle;
		}
	}

	return low;
}

float GetLineCursorEnd(const LineLayout& line) {
	if (line.glyphs.empty()) {
		return 0.0f;
	}
	const auto& glyph{ line.glyphs.back() };
	return glyph.position.x + glyph.advance;
}

std::optional<SourceCharacter> GetLastCharacter(const LineLayout& line) {
	if (line.glyphs.empty()) {
		return std::nullopt;
	}

	const auto& glyph{ line.glyphs.back() };
	return SourceCharacter{
		.codepoint				= glyph.codepoint,
		.run_index				= glyph.source_run_index,
		.source_codepoint_index = glyph.source_codepoint_index,
	};
}

float MeasureSyntheticCharacter(
	const impl::ResolvedStyledText& styled_text, const SourceCharacter& character, float cursor_x,
	std::optional<SourceCharacter>& previous, std::size_t tab_width, float scale
) {
	if (previous.has_value()) {
		cursor_x += GetPairSpacing(styled_text, previous.value(), character, scale);
	}
	cursor_x += GetCharacterAdvance(styled_text, character, cursor_x, tab_width, scale);
	previous  = character;
	return cursor_x;
}

void AppendSyntheticGlyph(
	LineLayout& line, const impl::ResolvedStyledText& styled_text, const SourceCharacter& character,
	float scale
) {
	PTGN_ASSERT(character.run_index < styled_text.runs.size());
	const auto& run{ styled_text.runs[character.run_index] };
	PTGN_ASSERT(run.font, "Valid font required for synthetic text glyph");

	auto previous{ GetLastCharacter(line) };
	float cursor_x{ GetLineCursorEnd(line) };

	if (previous.has_value()) {
		cursor_x += GetPairSpacing(styled_text, previous.value(), character, scale);
	}

	Glyph glyph;
	glyph.codepoint				 = character.codepoint;
	glyph.position				 = { cursor_x, line.baseline };
	glyph.texture				 = run.font->GetTexture();
	glyph.source_run_index		 = character.run_index;
	glyph.source_codepoint_index = character.source_codepoint_index;
	glyph.render_style			 = GetGlyphRenderStyle(run.style);

	if (auto metrics{ GetGlyphMetrics(run, character.codepoint) }) {
		float size{ run.style.size * scale };
		glyph.advance	 = metrics->advance * size;
		glyph.plane		 = metrics->plane;
		glyph.plane.min *= size;
		glyph.plane.max *= size;
		glyph.uv		 = metrics->uv;
	}

	line.glyphs.push_back(glyph);
	line.size.x		  = glyph.position.x + glyph.advance;
	line.bounds.max.x = line.bounds.min.x + line.size.x;
}

void EllipsizeLine(
	LineLayout& line, const impl::ResolvedStyledText& styled_text, const TextBox& box, float scale,
	std::optional<float> maximum_width
) {
	if (styled_text.runs.empty()) {
		line.glyphs.clear();
		line.size.x = 0.0f;
		return;
	}

	std::size_t run_index{ 0 };
	if (!line.glyphs.empty()) {
		run_index = line.glyphs.back().source_run_index;
	}

	SourceCharacter dot{
		.codepoint				= U'.',
		.run_index				= run_index,
		.source_codepoint_index = std::numeric_limits<std::size_t>::max(),
	};

	auto dots_fit = [&](std::size_t dot_count) {
		auto previous{ GetLastCharacter(line) };
		float width{ GetLineCursorEnd(line) };
		for (auto i{ 0uz }; i < dot_count; ++i) {
			width = MeasureSyntheticCharacter(
				styled_text, dot, width, previous, box.style.tab_width, scale
			);
		}
		return !maximum_width.has_value() || width <= maximum_width.value() ||
			   NearlyEqual(width, maximum_width.value());
	};

	while (!line.glyphs.empty() && !dots_fit(3)) {
		line.glyphs.pop_back();
		line.size.x = GetLineCursorEnd(line);
	}

	std::size_t dot_count{ 3 };
	while (dot_count > 0 && !dots_fit(dot_count)) {
		--dot_count;
	}

	for (auto i{ 0uz }; i < dot_count; ++i) {
		AppendSyntheticGlyph(line, styled_text, dot, scale);
	}

	line.size.x		  = GetLineCursorEnd(line);
	line.bounds.max.x = line.bounds.min.x + line.size.x;
}

void ApplyOverflow(
	TextLayout& layout, const impl::ResolvedStyledText& styled_text, const TextBox& box, float scale
) {
	if (layout.lines.empty()) {
		return;
	}

	std::size_t keep_lines{ layout.lines.size() };

	if (box.style.max_lines > 0) {
		keep_lines = std::min(keep_lines, box.style.max_lines);
	}

	if (box.style.overflow == OverflowMode::Ellipsis && box.HasHeight()) {
		float height{ 0.0f };
		std::size_t height_lines{ 0 };
		float box_height{ box.rect.GetSize().y };

		for (const auto& line : layout.lines) {
			if (height_lines > 0 && height + line.size.y > box_height &&
				!NearlyEqual(height + line.size.y, box_height)) {
				break;
			}
			height += line.size.y;
			++height_lines;
		}

		if (height_lines == 0 && !layout.lines.empty()) {
			height_lines = 1;
		}

		keep_lines = std::min(keep_lines, height_lines);
	}

	bool removed_lines{ keep_lines < layout.lines.size() };
	if (removed_lines) {
		layout.lines.resize(keep_lines);
		layout.truncated = true;
	}

	if (box.style.overflow != OverflowMode::Ellipsis || layout.lines.empty()) {
		RecalculateLayoutSize(layout);
		return;
	}

	std::optional<float> maximum_width;
	if (box.HasWidth()) {
		maximum_width = box.rect.GetSize().x;
	}

	for (auto line_index{ 0uz }; line_index < layout.lines.size(); ++line_index) {
		auto& line{ layout.lines[line_index] };
		bool horizontal_overflow{ maximum_width.has_value() &&
								  line.size.x > maximum_width.value() &&
								  !NearlyEqual(line.size.x, maximum_width.value()) };
		bool final_truncated_line{ removed_lines && line_index + 1 == layout.lines.size() };

		if (horizontal_overflow || final_truncated_line) {
			EllipsizeLine(line, styled_text, box, scale, maximum_width);
			layout.truncated = true;
		}
	}

	RecalculateLayoutSize(layout);
}

bool IsJustificationSpace(const Glyph& glyph) {
	return glyph.codepoint == U' ' || glyph.codepoint == U'\t';
}

void ApplyHorizontalAlignment(const TextBox& box, TextLayout& layout) {
	PTGN_ASSERT(
		box.style.alignment.horizontal.has_value(),
		"Text box must have a valid horizontal alignment set"
	);

	for (auto& line : layout.lines) {
		float x_offset{ 0.0f };
		float justify_extra_per_space{ 0.0f };

		if (box.HasWidth()) {
			float box_width{ box.rect.GetSize().x };

			switch (box.style.alignment.horizontal.value()) {
				using enum HorizontalAlign;

				case Left:	  x_offset = box.rect.min.x; break;
				case Center:  x_offset = box.rect.min.x + (box_width - line.size.x) * 0.5f; break;
				case Right:	  x_offset = box.rect.max.x - line.size.x; break;
				case Justify: {
					x_offset = box.rect.min.x;
					auto space_count{ static_cast<std::size_t>(
						std::ranges::count_if(line.glyphs, IsJustificationSpace)
					) };
					bool should_justify{ space_count > 0 &&
										 (!line.paragraph_end || box.style.justify_last_line) };

					if (float remaining{ box_width - line.size.x };
						should_justify && remaining > 0.0f) {
						justify_extra_per_space = remaining / static_cast<float>(space_count);
						line.size.x				= box_width;
					}
					break;
				}
			}
		} else {
			switch (box.style.alignment.horizontal.value()) {
				using enum HorizontalAlign;
				case Left:	  [[fallthrough]];
				case Justify: x_offset = 0.0f; break;
				case Center:  x_offset = -line.size.x * 0.5f; break;
				case Right:	  x_offset = -line.size.x; break;
			}
		}

		float justify_offset{ 0.0f };
		for (auto& glyph : line.glyphs) {
			glyph.position.x += x_offset + justify_offset;
			if (justify_extra_per_space > 0.0f && IsJustificationSpace(glyph)) {
				justify_offset += justify_extra_per_space;
			}
		}

		line.bounds.min.x = x_offset;
		line.bounds.max.x = x_offset + line.size.x;
	}
}

void ApplyVerticalAlignment(const TextBox& box, TextLayout& layout) {
	PTGN_ASSERT(
		box.style.alignment.vertical.has_value(),
		"Text box must have a valid horizontal alignment set"
	);

	float y_offset{ 0.0f };

	if (box.HasHeight()) {
		float box_height{ box.rect.GetSize().y };

		switch (box.style.alignment.vertical.value()) {
			using enum VerticalAlign;
			case Top:	 y_offset = box.rect.min.y; break;
			case Center: y_offset = box.rect.min.y + (box_height - layout.size.y) * 0.5f; break;
			case Bottom: y_offset = box.rect.max.y - layout.size.y; break;
		}
	} else {
		switch (box.style.alignment.vertical.value()) {
			using enum VerticalAlign;
			case Top:	 y_offset = 0.0f; break;
			case Center: y_offset = -layout.size.y * 0.5f; break;
			case Bottom: y_offset = -layout.size.y; break;
		}
	}

	if (NearlyEqual(y_offset, 0.0f)) {
		return;
	}

	for (auto& line : layout.lines) {
		line.baseline += y_offset;
		line.bounds	   = line.bounds.Translated({ 0.0f, y_offset });

		for (auto& glyph : line.glyphs) {
			glyph.position.y += y_offset;
		}
	}
}

void BuildDecorations(
	const impl::ResolvedStyledText& styled_text, float scale, TextLayout& layout
) {
	for (auto& line : layout.lines) {
		line.decorations.clear();

		auto begin{ line.glyphs.begin() };
		while (begin != line.glyphs.end()) {
			auto run_index{ begin->source_run_index };
			auto end{ begin };
			while (end != line.glyphs.end() && end->source_run_index == run_index) {
				++end;
			}

			if (run_index >= styled_text.runs.size()) {
				begin = end;
				continue;
			}

			const auto& run{ styled_text.runs[run_index] };
			const auto& style{ run.style };
			bool underline{ HasFontFlag(style.flags, FontStyle::Underline) };
			bool strikethrough{ HasFontFlag(style.flags, FontStyle::Strikethrough) };

			if (!underline && !strikethrough) {
				begin = end;
				continue;
			}

			float size{ style.size * scale };
			float thickness{ std::max(1.0f, size * 0.065f) };
			float x_min{ begin->position.x };
			float x_max{ begin->position.x };

			for (auto it{ begin }; it != end; ++it) {
				x_min = std::min(x_min, it->position.x);
				x_max = std::max(x_max, it->position.x + it->advance);
			}

			if (underline) {
				float y{ line.baseline + size * 0.12f };
				line.decorations.emplace_back(
					TextDecoration{
						.type			  = TextDecorationType::Underline,
						.rect			  = Rect{ { x_min, y }, { x_max, y + thickness } },
						.color			  = style.color,
						.source_run_index = run_index,
					}
				);
			}

			if (strikethrough) {
				float y{ line.baseline - size * 0.28f };
				line.decorations.emplace_back(
					TextDecoration{
						.type			  = TextDecorationType::Strikethrough,
						.rect			  = Rect{ { x_min, y }, { x_max, y + thickness } },
						.color			  = style.color,
						.source_run_index = run_index,
					}
				);
			}

			begin = end;
		}
	}
}

void AssignVisibleOrder(TextLayout& layout) {
	std::size_t order{ 0 };
	for (auto& line : layout.lines) {
		for (auto& glyph : line.glyphs) {
			glyph.visible_order = order++;
		}
	}
}

TextBatchStyle GetGlyphBatchStyle(const TextLayout& layout, const Glyph& glyph) {
	PTGN_ASSERT(
		glyph.source_run_index < layout.batch_styles.size(),
		"Glyph source run index does not have a matching text batch style"
	);

	const auto& style{ layout.batch_styles[glyph.source_run_index] };
	PTGN_ASSERT(style.texture == glyph.texture, "Glyph texture does not match its source run");
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

V2_float GetEffectOffset(const Glyph& glyph, float time) {
	const auto& effect{ glyph.render_style.effect };
	auto order{ static_cast<float>(glyph.visible_order) };
	float phase{ effect.phase + order * kGlyphEffectPhaseStep };
	float t{ time * effect.speed + phase };

	auto oscillate = [&]<typename TOscillation>(const TOscillation& oscillation) {
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
		case Wave:	 return { 0.0f, std::sin(t * effect.frequency) * effect.amplitude };
		case Shake:	 return oscillate(kShakeOscillation);
		case Pulse:	 [[fallthrough]];
		case None:	 [[fallthrough]];
		default:	 return {};
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

bool LinePassesClips(const LineLayout& line, std::span<const TextClip> clips) {
	for (const auto& clip : clips) {
		if (clip.mode == TextClipMode::None) {
			continue;
		}

		PTGN_ASSERT(
			clip.rect.GetSize().IsPositive(), "Text clip rectangle must have positive area"
		);

		bool keep{ true };
		switch (clip.mode) {
			using enum TextClipMode;
			case None:		  keep = true; break;
			case Clip:		  keep = impl::RectContainsRect(clip.rect, line.bounds); break;
			case ClipPartial: keep = impl::Intersects(line.bounds, clip.rect); break;
		}

		if (!keep) {
			return false;
		}
	}

	return true;
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

void EmitGlyphQuad(
	const Glyph& glyph, Color tint, Depth depth, int entity_id, float time,
	std::vector<impl::TextureQuad>& quads
) {
	if (!glyph.plane.GetSize().IsPositive()) {
		return;
	}

	auto effect_offset{ GetEffectOffset(glyph, time) };
	auto quad_min{ glyph.position + glyph.plane.min + effect_offset };
	auto quad_max{ glyph.position + glyph.plane.max + effect_offset };

	if (float effect_scale{ GetEffectScale(glyph, time) }; !NearlyEqual(effect_scale, 1.0f)) {
		auto center{ (quad_min + quad_max) * 0.5f };
		quad_min = center + (quad_min - center) * effect_scale;
		quad_max = center + (quad_max - center) * effect_scale;
	}

	std::array positions{
		quad_min,
		V2_float{ quad_max.x, quad_min.y },
		quad_max,
		V2_float{ quad_min.x, quad_max.y },
	};

	if (HasFontFlag(glyph.render_style.flags, FontStyle::Italic)) {
		ApplyItalicShear(positions);
	}

	std::array tex_coords{
		glyph.uv.min,
		V2_float{ glyph.uv.max.x, glyph.uv.min.y },
		glyph.uv.max,
		V2_float{ glyph.uv.min.x, glyph.uv.max.y },
	};

	auto color{ Color::Multiply(glyph.render_style.color, tint) };
	quads.emplace_back(
		impl::CreateTextureQuad(positions, depth, color.Normalized(), tex_coords, entity_id)
	);
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

	quads.emplace_back(
		impl::CreateTextureQuad(positions, depth, color.Normalized(), tex_coords, entity_id)
	);
}

} // namespace

namespace impl {

TextLayout BuildTextLayout(const ResolvedStyledText& styled_text, const TextBox& box) {
	PTGN_ASSERT(box.style.tab_width > 0, "Text tab width must be at least one space");

	PTGN_ASSERT(
		box.style.alignment.horizontal.has_value(),
		"Text box must have a valid horizontal alignment set"
	);
	PTGN_ASSERT(
		box.style.alignment.vertical.has_value(),
		"Text box must have a valid horizontal alignment set"
	);

	auto characters{ BuildSourceCharacters(styled_text, box.style.collapse_spaces) };
	auto tokens{ Tokenize(characters) };

	float scale{ 1.0f };
	if (box.style.overflow == OverflowMode::ScaleToFit) {
		scale = FindBestScale(styled_text, box, characters, tokens);
	}

	auto layout{ BuildLinesAtScale(styled_text, box, characters, tokens, scale) };

	ApplyOverflow(layout, styled_text, box, scale);
	ApplyHorizontalAlignment(box, layout);
	ApplyVerticalAlignment(box, layout);
	BuildDecorations(styled_text, scale, layout);
	AssignVisibleOrder(layout);
	RecalculateLayoutSize(layout);

	layout.built_alignment = box.style.alignment;
	layout.dirty		   = false;

	return layout;
}

TextMeasurement MeasureText(const ResolvedStyledText& styled_text, const TextBox& box) {
	auto layout{ BuildTextLayout(styled_text, box) };
	return {
		.size			   = layout.size,
		.line_count		   = layout.lines.size(),
		.truncated		   = layout.truncated,
		.used_shrink_scale = layout.used_shrink_scale,
	};
}

std::vector<UniformWrite> GetTextUniforms(const DistanceFieldStyle& sdf, bool is_decoration) {
	PTGN_ASSERT(sdf.pixel_range > 0.0f, "Invalid font pixel range");

	return {
		{ "u_Weight", sdf.weight },
		{ "u_Softness", sdf.softness },

		{ "u_OutlineColor", sdf.outline.color.Normalized() },
		{ "u_OutlineWidth", sdf.outline.width },
		{ "u_OutlineSoftness", sdf.outline.softness },

		{ "u_ShadowColor", sdf.shadow.color.Normalized() },
		{ "u_ShadowOffset", sdf.shadow_offset },
		{ "u_ShadowWidth", sdf.shadow.width },
		{ "u_ShadowSoftness", sdf.shadow.softness },

		{ "u_OuterGlowColor", sdf.outer_glow.color.Normalized() },
		{ "u_OuterGlowWidth", sdf.outer_glow.width },
		{ "u_OuterGlowSoftness", sdf.outer_glow.softness },

		{ "u_InnerGlowColor", sdf.inner_glow.color.Normalized() },
		{ "u_InnerGlowWidth", sdf.inner_glow.width },
		{ "u_InnerGlowSoftness", sdf.inner_glow.softness },

		{ "u_PixelRange", sdf.pixel_range },
		{ "u_IsDecoration", is_decoration ? 1.0f : 0.0f },
	};
}

std::vector<TextDrawBatch> BuildTextDrawBatches(const DrawTextRequest& request) {
	std::vector<TextDrawBatch> batches;

	for (const auto& line : request.layout.lines) {
		if (!LinePassesClips(line, request.clips)) {
			continue;
		}

		for (const auto& glyph : line.glyphs) {
			if (glyph.visible_order >= request.reveal_glyph_count) {
				continue;
			}

			if (!glyph.plane.GetSize().IsPositive()) {
				continue;
			}

			auto style{ GetGlyphBatchStyle(request.layout, glyph) };
			auto& batch{ GetOrCreateTextBatch(batches, style, false) };
			EmitGlyphQuad(
				glyph, request.tint, request.depth, request.entity_id, request.time, batch.quads
			);
		}
	}

	for (const auto& line : request.layout.lines) {
		if (!LinePassesClips(line, request.clips)) {
			continue;
		}

		for (const auto& decoration : line.decorations) {
			PTGN_ASSERT(
				decoration.source_run_index < request.layout.batch_styles.size(),
				"Decoration source run index does not have a matching text batch style"
			);

			const auto& style{ request.layout.batch_styles[decoration.source_run_index] };
			auto& batch{ GetOrCreateTextBatch(batches, style, true) };
			EmitDecorationQuad(
				decoration, request.tint, request.depth, request.entity_id, batch.quads
			);
		}
	}

	return batches;
}

bool TextLayoutFitsInBox(const TextLayout& layout, Rect box) {
	auto box_size{ box.GetSize() };
	bool fits_width{ box_size.x <= 0.0f || layout.size.x <= box_size.x ||
					 NearlyEqual(layout.size.x, box_size.x) };
	bool fits_height{ box_size.y <= 0.0f || layout.size.y <= box_size.y ||
					  NearlyEqual(layout.size.y, box_size.y) };
	return fits_width && fits_height;
}

PreparedTextDraw PrepareTextDraw(
	Transform transform, const TextLayout& layout, const TextBox& box, Origin origin,
	std::optional<TextClip> explicit_clip
) {
	PreparedTextDraw result;
	result.transform = transform;

	// An explicit text box is the positioning reference. For unboxed text,
	// the generated logical bounds act as the implicit text box.
	Rect origin_rect{ box.HasBox() ? box.rect : layout.GetBounds() };

	result.transform.Translate(-origin_rect.GetOriginPoint(origin));

	auto add_clip = [&](Rect rect, TextClipMode mode) {
		if (mode == TextClipMode::None) {
			return;
		}

		if (!rect.GetSize().IsPositive()) {
			result.drawable = false;
			return;
		}

		PTGN_ASSERT(result.clip_count < result.clips.size());

		result.clips[result.clip_count] = TextClip{
			.rect = rect,
			.mode = mode,
		};

		++result.clip_count;
	};

	if (box.HasArea()) {
		switch (box.style.overflow) {
			using enum OverflowMode;

			case Clip:		  add_clip(box.rect, TextClipMode::Clip); break;

			case ClipPartial: add_clip(box.rect, TextClipMode::ClipPartial); break;

			case Overflow:	  [[fallthrough]];
			case Ellipsis:	  [[fallthrough]];
			case ScaleToFit:  break;
		}
	}

	if (explicit_clip.has_value()) {
		add_clip(explicit_clip->rect, explicit_clip->mode);
	}

	return result;
}

} // namespace impl

} // namespace ptgn
