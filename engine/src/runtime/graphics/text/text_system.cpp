#include "runtime/graphics/text/text_system.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/renderer.h"
#include "renderer/vertex/vertex.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/text/font_data.h"
#include "runtime/graphics/text/text_builder.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_layout.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

[[nodiscard]] static bool IsWhitespace(std::uint32_t cp) {
	return cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r';
}

[[nodiscard]] static std::uint32_t GetNextCodepoint(std::u32string_view text, std::size_t index) {
	if (index + 1 < text.size()) {
		return text[index + 1];
	}
	return 0;
}

bool TextLayoutCache::TryGet(const TextLayoutKey& key, TextLayout* layout) const {
	auto it{ cache_.find(key) };
	if (it == cache_.end()) {
		return false;
	}
	if (layout != nullptr) {
		*layout = it->second;
	}
	return true;
}

void TextLayoutCache::Put(const TextLayoutKey& key, TextLayout layout) {
	cache_[key] = std::move(layout);
}

void TextLayoutCache::Clear() {
	cache_.clear();
}

StyledText TextSystem::MakePlainText(std::string_view text, const TextRunStyle& run_style) {
	StyledText styled;
	TextRun run;
	run.text  = std::string{ text };
	run.style = run_style;
	styled.runs.push_back(std::move(run));
	return styled;
}

TextLayoutKey TextSystem::MakeKey(TextLayoutRequest request) {
	TextLayoutKey key;
	key.rich_text_hash				 = Hash(request.styled_text);
	key.box_width_q					 = QuantizeUnsigned(request.box.rect.GetSize().x);
	key.box_height_q				 = QuantizeUnsigned(request.box.rect.GetSize().y);
	key.min_shrink_scale_q			 = QuantizeUnsigned(request.box.style.min_shrink_scale);
	key.max_shrink_scale_q			 = QuantizeUnsigned(request.box.style.max_shrink_scale);
	key.horizontal_align			 = request.box.style.horizontal_align;
	key.vertical_align				 = request.box.style.vertical_align;
	key.wrap_mode					 = request.box.style.wrap_mode;
	key.overflow_mode				 = request.box.style.overflow_mode;
	key.collapse_spaces				 = request.box.style.collapse_spaces;
	key.justify_last_line			 = request.box.style.justify_last_line;
	key.allow_word_break_in_overflow = request.box.style.allow_word_break_in_overflow;
	key.max_lines_q					 = static_cast<std::uint32_t>(request.box.style.max_lines);
	key.ellipsis_on_max_lines		 = request.box.style.ellipsis_on_max_lines;

	return key;
}

TextLayout TextSystem::BuildLayout(TextLayoutRequest request) {
	TextLayoutKey key{ MakeKey(request) };
	TextLayout cached;
	if (cache_.TryGet(key, &cached)) {
		return cached;
	}

	float shrink{ 1.0f };
	if (request.box.style.overflow_mode == OverflowMode::ShrinkToFit) {
		shrink = FindBestShrinkScale(request.styled_text, request.box);
	}

	CandidateLayout candidate{ BuildSinglePassLayout(request.styled_text, request.box, shrink) };
	TextLayout layout{ std::move(candidate.layout) };
	layout.used_shrink_scale = candidate.used_shrink_scale;

	if (request.box.style.max_lines > 0 && layout.lines.size() > request.box.style.max_lines) {
		layout.truncated_by_max_lines = true;
		if (request.box.style.ellipsis_on_max_lines) {
			ApplyEllipsisForMaxLines(request.styled_text, request.box, shrink, &layout);
		} else {
			std::size_t last_line_index{ request.box.style.max_lines - 1 };
			std::size_t hide_from{ layout.lines[last_line_index].glyph_end };
			for (std::size_t i{ hide_from }; i < layout.glyphs.size(); ++i) {
				layout.glyphs[i].visible = false;
			}
			layout.lines.resize(request.box.style.max_lines);
			layout.measured_size.y = static_cast<float>(layout.lines.size()) *
									 (layout.lines.empty() ? 0.0f : layout.lines.front().size.y);
		}
	}

	if (request.box.style.overflow_mode == OverflowMode::Clip) {
		ApplyClipVisibility(request.box.rect, &layout);
		layout.clipped = true;
	}

	ApplyVerticalAlignment(request.box, &layout);

	cache_.Put(key, layout);

	return layout;
}

TextLayout TextSystem::BuildLayout(
	std::string_view text, impl::FontData& font, Rect rect, TextLayoutStyle style,
	TextRunStyle run_style
) {
	run_style.font = &font;
	TextLayoutRequest request;
	request.styled_text = MakePlainText(text, run_style);
	request.box.rect	= rect;
	request.box.style	= style;
	return BuildLayout(std::move(request));
}

TextMeasurement TextSystem::Measure(TextLayoutRequest request) {
	TextLayout layout{ BuildLayout(std::move(request)) };

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

TextMeasurement TextSystem::Measure(
	std::string_view text, impl::FontData& font, Rect rect, TextLayoutStyle style,
	TextRunStyle run_style
) {
	run_style.font = &font;
	TextLayoutRequest request;
	request.styled_text = MakePlainText(text, run_style);
	request.box.rect	= rect;
	request.box.style	= style;
	return Measure(std::move(request));
}

void TextSystem::BuildVertices(
	TextLayout& layout, TextVertexBuildParams params, std::vector<impl::TextureVertex>& vertices,
	std::vector<std::uint32_t>& indices
) const {
	for (GlyphInstance& glyph : layout.glyphs) {
		if (!glyph.visible) {
			continue;
		}
		if (glyph.visible_order >= params.reveal_glyph_count) {
			continue;
		}

		if (params.clip_rect.has_value()) {
			V2_float gmin{ glyph.position + glyph.plane.GetMin() };
			V2_float gmax{ glyph.position + glyph.plane.GetMax() };

			if (gmax.x <= params.clip_rect->GetMin().x || gmin.x >= params.clip_rect->GetMax().x ||
				gmax.y <= params.clip_rect->GetMin().y || gmin.y >= params.clip_rect->GetMax().y) {
				continue;
			}
		}

		EmitGlyphQuad(glyph, params, vertices, indices);
	}
}

std::u32string TextSystem::DecodeUtf8(std::string_view text) {
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

	std::size_t i{ 0 };
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

std::uint32_t TextSystem::QuantizeUnsigned(float value, float scale) {
	return static_cast<std::uint32_t>(std::lround(value * scale));
}

int32_t TextSystem::QuantizeSigned(float value, float scale) {
	return static_cast<int32_t>(std::lround(value * scale));
}

std::vector<RichTextToken> TextSystem::Tokenize(StyledText& styled_text, float global_shrink) {
	std::vector<RichTextToken> tokens{};

	for (std::size_t run_index{}; run_index < styled_text.runs.size(); ++run_index) {
		const auto& run{ styled_text.runs[run_index] };
		std::u32string decoded{ DecodeUtf8(run.text) };

		std::size_t i{ 0 };
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
				++i;
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
				continue;
			}

			if (cp == U'\t') {
				RichTextToken token;
				token.type		= RichTextToken::Type::Tab;
				token.run_index = run_index;
				token.text.push_back(U'\t');
				token.width = MeasureTokenWidth(token, styled_text, global_shrink);
				tokens.push_back(std::move(token));
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
		}
	}

	return tokens;
}

float TextSystem::MeasureLineHeight(TextRunStyle& style) {
	auto font{ style.GetFont() };
	if (font == nullptr) {
		return 0.0f;
	}

	impl::FontMetrics metrics{ font->GetFontMetrics() };
	return (metrics.line_height + style.line_spacing) * style.scale;
}

float TextSystem::MeasureTokenWidth(
	RichTextToken& token, StyledText& styled_text, float global_shrink
) {
	if (token.run_index >= styled_text.runs.size()) {
		return 0.0f;
	}

	const TextRun& run{ styled_text.runs[token.run_index] };
	auto font{ run.style.GetFont() };
	if (font == nullptr) {
		return 0.0f;
	}

	float scale{ run.style.scale * global_shrink };

	if (token.type == RichTextToken::Type::Tab) {
		float space_adv{ font->GetAdvance(U' ', 0) };
		return (space_adv + run.style.kerning + run.style.tracking) * scale * 4.0f;
	}

	float width{ 0.0f };
	for (std::size_t i{ 0 }; i < token.text.size(); ++i) {
		std::uint32_t cp{ token.text[i] };
		std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
		width += (font->GetAdvance(cp, next_cp) + run.style.kerning + run.style.tracking) * scale;
	}

	return width;
}

bool TextSystem::FitsInBox(TextLayout& layout, Rect box) {
	return layout.measured_size.x <= box.GetSize().x && layout.measured_size.y <= box.GetSize().y;
}

std::optional<TextSystem::ResolvedGlyph> TextSystem::ResolveGlyph(
	TextRun& run, std::uint32_t codepoint, std::uint32_t next_codepoint,
	std::size_t source_run_index, std::size_t source_codepoint_index, float global_shrink
) {
	auto font{ run.style.GetFont() };
	if (font == nullptr) {
		return std::nullopt;
	}

	std::optional<impl::GlyphMetrics> metrics{ font->GetGlyph(codepoint) };
	if (!metrics.has_value()) {
		metrics = font->GetGlyph(U'?');
	}
	if (!metrics.has_value()) {
		return std::nullopt;
	}

	ResolvedGlyph resolved{};
	resolved.codepoint				= codepoint;
	resolved.metrics				= *metrics;
	resolved.texture_index			= font->GetAtlasTextureIndex();
	resolved.source_run_index		= source_run_index;
	resolved.source_codepoint_index = source_codepoint_index;

	resolved.render_style.color			   = run.style.color;
	resolved.render_style.effect.type	   = run.style.effect.type;
	resolved.render_style.effect.amplitude = run.style.effect.amplitude;
	resolved.render_style.effect.frequency = run.style.effect.frequency;
	resolved.render_style.effect.speed	   = run.style.effect.speed;
	resolved.render_style.effect.phase	   = run.style.effect.phase;

	resolved.metrics.plane.GetMin() *= run.style.scale * global_shrink;
	resolved.metrics.plane.GetMax() *= run.style.scale * global_shrink;
	resolved.metrics.advance =
		(font->GetAdvance(codepoint, next_codepoint) + run.style.kerning + run.style.tracking) *
		(run.style.scale * global_shrink);

	return resolved;
}

TextSystem::CandidateLayout TextSystem::BuildSinglePassLayout(
	StyledText& styled_text, TextBox box, float global_shrink
) {
	std::vector<RichTextToken> tokens{ Tokenize(styled_text, global_shrink) };

	TextLayout layout;
	layout.used_shrink_scale = global_shrink;

	std::vector<GlyphInstance> current_line_glyphs;
	V2_float current_line_size;
	float y{ 0.0f };
	std::size_t visible_order{ 0 };

	auto flush_line = [&](bool ends_with_explicit_newline) {
		if (current_line_glyphs.empty() && !ends_with_explicit_newline) {
			return;
		}

		LineLayout line;
		line.glyph_begin				= layout.glyphs.size();
		line.glyph_end					= layout.glyphs.size() + current_line_glyphs.size();
		line.size						= current_line_size;
		line.baseline_y					= y;
		line.ends_with_explicit_newline = ends_with_explicit_newline;

		for (const GlyphInstance& glyph : current_line_glyphs) {
			if (glyph.codepoint == U' ' || glyph.codepoint == U'\t') {
				++line.justify_space_count;
			}
		}

		float x_offset{ 0.0f };
		switch (box.style.horizontal_align) {
			using enum HorizontalAlign;
			case Left: x_offset = box.rect.GetMin().x; break;
			case Center:
				x_offset = box.rect.GetMin().x + (box.rect.GetSize().x - line.size.x) * 0.5f;
				break;
			case Right:
				x_offset = box.rect.GetMin().x + (box.rect.GetSize().x - line.size.x);
				break;
			case Justify:
				x_offset = box.rect.GetMin().x;
				if (line.justify_space_count > 0 &&
					(!ends_with_explicit_newline || box.style.justify_last_line)) {
					line.justify_extra_per_space = (box.rect.GetSize().x - line.size.x) /
												   static_cast<float>(line.justify_space_count);
				}
				break;
		}

		float justify_extra{ 0.0f };
		for (GlyphInstance& glyph : current_line_glyphs) {
			glyph.position.x	+= x_offset + justify_extra;
			glyph.position.y	+= box.rect.GetMax().y;
			glyph.line_index	 = layout.lines.size();
			glyph.visible_order	 = visible_order++;

			if (box.style.horizontal_align == HorizontalAlign::Justify &&
				line.justify_extra_per_space > 0.0f &&
				(glyph.codepoint == U' ' || glyph.codepoint == U'\t')) {
				justify_extra += line.justify_extra_per_space;
			}
		}

		layout.glyphs.insert(
			layout.glyphs.end(), current_line_glyphs.begin(), current_line_glyphs.end()
		);
		layout.lines.push_back(line);

		layout.measured_size.x	= std::max(layout.measured_size.x, line.size.x);
		layout.measured_size.y += line.size.y;

		current_line_glyphs.clear();
		current_line_size  = {};
		y				  -= line.size.y;
	};

	for (RichTextToken& token : tokens) {
		if (token.type == RichTextToken::Type::Newline) {
			flush_line(true);
			continue;
		}

		if (bool wrap_here{ box.style.wrap_mode != WrapMode::None && current_line_size.x > 0.0f &&
							current_line_size.x + token.width > box.rect.GetSize().x };
			wrap_here && !(token.type == RichTextToken::Type::Word &&
						   box.style.wrap_mode == WrapMode::Character &&
						   box.style.allow_word_break_in_overflow)) {
			flush_line(false);
		}

		TextRun& run{ styled_text.runs[token.run_index] };
		float line_h{ MeasureLineHeight(run.style) * global_shrink };
		current_line_size.y = std::max(current_line_size.y, line_h);

		if (token.type == RichTextToken::Type::Space || token.type == RichTextToken::Type::Tab) {
			if (std::optional<ResolvedGlyph> space_glyph{ ResolveGlyph(
					run, token.type == RichTextToken::Type::Space ? U' ' : U'\t', 0,
					token.run_index, 0, global_shrink
				) };
				space_glyph.has_value()) {
				GlyphInstance glyph;
				glyph.codepoint				 = space_glyph->codepoint;
				glyph.position				 = { current_line_size.x, y };
				glyph.plane					 = space_glyph->metrics.plane;
				glyph.uv					 = space_glyph->metrics.uv;
				glyph.texture_index			 = space_glyph->texture_index;
				glyph.source_run_index		 = token.run_index;
				glyph.source_codepoint_index = 0;
				glyph.render_style			 = space_glyph->render_style;
				current_line_glyphs.push_back(glyph);
			}

			current_line_size.x += token.width;
			continue;
		}

		if (token.type == RichTextToken::Type::Word && token.width > box.rect.GetSize().x &&
			box.style.wrap_mode == WrapMode::Character && box.style.allow_word_break_in_overflow) {
			for (std::size_t i{ 0 }; i < token.text.size(); ++i) {
				std::uint32_t cp{ token.text[i] };
				std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
				std::optional<ResolvedGlyph> resolved{
					ResolveGlyph(run, cp, next_cp, token.run_index, i, global_shrink)
				};
				if (!resolved.has_value()) {
					continue;
				}

				if (current_line_size.x > 0.0f &&
					current_line_size.x + resolved->metrics.advance > box.rect.GetSize().x) {
					flush_line(false);
					current_line_size.y = std::max(current_line_size.y, line_h);
				}

				GlyphInstance glyph;
				glyph.codepoint				 = resolved->codepoint;
				glyph.position				 = { current_line_size.x, y };
				glyph.plane					 = resolved->metrics.plane;
				glyph.uv					 = resolved->metrics.uv;
				glyph.texture_index			 = resolved->texture_index;
				glyph.source_run_index		 = resolved->source_run_index;
				glyph.source_codepoint_index = resolved->source_codepoint_index;
				glyph.render_style			 = resolved->render_style;
				current_line_glyphs.push_back(glyph);

				current_line_size.x += resolved->metrics.advance;
			}
			continue;
		}

		float x{ current_line_size.x };
		for (std::size_t i{ 0 }; i < token.text.size(); ++i) {
			std::uint32_t cp{ token.text[i] };
			std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
			std::optional<ResolvedGlyph> resolved{
				ResolveGlyph(run, cp, next_cp, token.run_index, i, global_shrink)
			};
			if (!resolved.has_value()) {
				continue;
			}

			GlyphInstance glyph;
			glyph.codepoint				 = resolved->codepoint;
			glyph.position				 = { x, y };
			glyph.plane					 = resolved->metrics.plane;
			glyph.uv					 = resolved->metrics.uv;
			glyph.texture_index			 = resolved->texture_index;
			glyph.source_run_index		 = resolved->source_run_index;
			glyph.source_codepoint_index = resolved->source_codepoint_index;
			glyph.render_style			 = resolved->render_style;
			current_line_glyphs.push_back(glyph);

			x += resolved->metrics.advance;
		}

		current_line_size.x = x;
	}

	flush_line(false);

	if (!styled_text.runs.empty()) {
		layout.batch_style = styled_text.runs.front().style.sdf;

		if (styled_text.runs.front().style.IsUsingFakeBold()) {
			layout.batch_style.weight += styled_text.runs.front().style.fake_bold_weight;
		}
	}

	CandidateLayout result;
	result.layout			 = std::move(layout);
	result.used_shrink_scale = global_shrink;
	return result;
}

float TextSystem::FindBestShrinkScale(StyledText& styled_text, TextBox box) {
	float lo{ box.style.min_shrink_scale };
	float hi{ box.style.max_shrink_scale };
	float best{ lo };

	for (int i{ 0 }; i < 16; ++i) {
		float mid{ 0.5f * (lo + hi) };
		CandidateLayout candidate{ BuildSinglePassLayout(styled_text, box, mid) };
		if (FitsInBox(candidate.layout, box.rect)) {
			best = mid;
			lo	 = mid;
		} else {
			hi = mid;
		}
	}

	return best;
}

void TextSystem::ApplyVerticalAlignment(TextBox box, TextLayout* layout) {
	if (layout == nullptr) {
		return;
	}

	float offset_y{ 0.0f };

	switch (box.style.vertical_align) {
		using enum VerticalAlign;
		case Top:	 offset_y = 0.0f; break;
		case Center: offset_y = -(box.rect.GetSize().y - layout->measured_size.y) * 0.5f; break;
		case Bottom: offset_y = -(box.rect.GetSize().y - layout->measured_size.y); break;
	}

	for (GlyphInstance& glyph : layout->glyphs) {
		glyph.position.y += offset_y;
	}

	layout->content_offset = { 0.0f, offset_y };
}

void TextSystem::ApplyEllipsisForMaxLines(
	StyledText& styled_text, TextBox box, float global_shrink, TextLayout* layout
) {
	if (layout == nullptr) {
		return;
	}

	if (box.style.max_lines == 0 || layout->lines.size() <= box.style.max_lines) {
		return;
	}

	std::size_t keep_lines{ box.style.max_lines };
	std::size_t last_visible_line_index{ keep_lines - 1 };
	const LineLayout& last_line{ layout->lines[last_visible_line_index] };

	std::size_t hide_from{ last_line.glyph_end };
	for (std::size_t i{ hide_from }; i < layout->glyphs.size(); ++i) {
		layout->glyphs[i].visible = false;
	}

	TextRun* source_run{ nullptr };
	if (last_line.glyph_begin < layout->glyphs.size()) {
		const GlyphInstance& anchor{ layout->glyphs[last_line.glyph_begin] };
		if (anchor.source_run_index < styled_text.runs.size()) {
			source_run = &styled_text.runs[anchor.source_run_index];
		}
	}

	if (source_run == nullptr) {
		layout->lines.resize(keep_lines);
		layout->ellipsized			   = true;
		layout->truncated_by_max_lines = true;
		return;
	}

	auto font{ source_run->style.GetFont() };
	if (font == nullptr) {
		layout->lines.resize(keep_lines);
		layout->ellipsized			   = true;
		layout->truncated_by_max_lines = true;
		return;
	}

	std::u32string dots{ U"..." };
	float dots_width{ 0.0f };
	for (std::size_t i{ 0 }; i < dots.size(); ++i) {
		std::uint32_t cp{ dots[i] };
		std::uint32_t next_cp{ GetNextCodepoint(dots, i) };
		dots_width += (font->GetAdvance(cp, next_cp) + source_run->style.kerning +
					   source_run->style.tracking) *
					  (source_run->style.scale * global_shrink);
	}

	std::size_t cutoff{ last_line.glyph_end };
	float usable_x{ box.rect.GetMin().x + box.rect.GetSize().x - dots_width };

	for (std::size_t i{ last_line.glyph_begin }; i < last_line.glyph_end; ++i) {
		GlyphInstance& glyph{ layout->glyphs[i] };
		float right{ glyph.position.x + glyph.plane.GetMax().x };
		if (right > usable_x) {
			cutoff = i;
			break;
		}
	}

	for (std::size_t i{ cutoff }; i < last_line.glyph_end; ++i) {
		layout->glyphs[i].visible = false;
	}

	float start_x{ box.rect.GetMin().x };
	if (cutoff > last_line.glyph_begin) {
		GlyphInstance& prev{ layout->glyphs[cutoff - 1] };
		start_x = prev.position.x + prev.plane.GetMax().x;
	}

	float y{ layout->glyphs[last_line.glyph_begin].position.y };

	for (std::size_t i{ 0 }; i < dots.size(); ++i) {
		std::uint32_t cp{ dots[i] };
		std::uint32_t next_cp{ GetNextCodepoint(dots, i) };
		std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
			*source_run, cp, next_cp,
			last_line.glyph_begin < layout->glyphs.size()
				? layout->glyphs[last_line.glyph_begin].source_run_index
				: 0,
			i, global_shrink
		) };
		if (!resolved.has_value()) {
			continue;
		}

		GlyphInstance glyph;
		glyph.codepoint				 = resolved->codepoint;
		glyph.position				 = { start_x, y };
		glyph.plane					 = resolved->metrics.plane;
		glyph.uv					 = resolved->metrics.uv;
		glyph.texture_index			 = resolved->texture_index;
		glyph.source_run_index		 = resolved->source_run_index;
		glyph.source_codepoint_index = resolved->source_codepoint_index;
		glyph.render_style			 = resolved->render_style;
		glyph.line_index			 = last_visible_line_index;
		glyph.visible_order			 = layout->glyphs.size();
		layout->glyphs.push_back(glyph);

		start_x += resolved->metrics.advance;
	}

	layout->lines.resize(keep_lines);
	layout->ellipsized			   = true;
	layout->truncated_by_max_lines = true;
	layout->measured_size.y =
		static_cast<float>(layout->lines.size()) * layout->lines.front().size.y;
}

void TextSystem::ApplyClipVisibility(Rect clip_rect, TextLayout* layout) {
	if (layout == nullptr) {
		return;
	}

	for (GlyphInstance& glyph : layout->glyphs) {
		V2_float gmin{ glyph.position + glyph.plane.GetMin() };
		V2_float gmax{ glyph.position + glyph.plane.GetMax() };

		if (gmax.x <= clip_rect.GetMin().x || gmin.x >= clip_rect.GetMax().x ||
			gmax.y <= clip_rect.GetMin().y || gmin.y >= clip_rect.GetMax().y) {
			glyph.visible = false;
		}
	}
}

void TextSystem::EmitGlyphQuad(
	GlyphInstance& glyph, const TextVertexBuildParams& params,
	std::vector<impl::TextureVertex>& vertices, std::vector<std::uint32_t>& indices
) {
	V2_float effect_offset{ glyph.GetEffectOffset(params.time) };

	V2_float quad_min{ glyph.position + glyph.plane.GetMin() + effect_offset };
	V2_float quad_max{ glyph.position + glyph.plane.GetMax() + effect_offset };

	if (float scale{ glyph.GetEffectScale(params.time) }; !NearlyEqual(scale, 1.0f)) {
		V2_float center = (quad_min + quad_max) * 0.5f;
		quad_min		= center + (quad_min - center) * scale;
		quad_max		= center + (quad_max - center) * scale;
	}

	auto color_n{ glyph.render_style.color.Normalized() };

	auto base_index{ static_cast<std::uint32_t>(vertices.size()) };

	impl::TextureVertex v0{
		{ quad_min.x, quad_min.y },
		params.depth,
		color_n,
		glyph.uv.GetMin(),
		static_cast<float>(glyph.texture_index),
		params.entity_id,
	};

	impl::TextureVertex v1{
		{ quad_min.x, quad_max.y },
		params.depth,
		color_n,
		{ glyph.uv.GetMin().x, glyph.uv.GetMax().y },
		static_cast<float>(glyph.texture_index),
		params.entity_id,
	};

	impl::TextureVertex v2{
		{ quad_max.x, quad_max.y },
		params.depth,
		color_n,
		glyph.uv.GetMax(),
		static_cast<float>(glyph.texture_index),
		params.entity_id,
	};

	impl::TextureVertex v3{
		{ quad_max.x, quad_min.y },
		params.depth,
		color_n,
		{ glyph.uv.GetMax().x, glyph.uv.GetMin().y },
		static_cast<float>(glyph.texture_index),
		params.entity_id,
	};

	vertices.emplace_back(v0);
	vertices.emplace_back(v1);
	vertices.emplace_back(v2);
	vertices.emplace_back(v0);
	vertices.emplace_back(v2);
	vertices.emplace_back(v3);

	indices.push_back(base_index + 0);
	indices.push_back(base_index + 1);
	indices.push_back(base_index + 2);
	indices.push_back(base_index + 2);
	indices.push_back(base_index + 3);
	indices.push_back(base_index + 0);
}

void TextSystem::RenderText(
	DrawContext& renderer, const TextLayoutRequest& request, float depth, int entity_id,
	std::optional<Rect> clip_rect, std::size_t reveal_glyph_count
) {
	TextLayout layout = BuildLayout(request);

	const DistanceFieldStyle& style{ layout.batch_style };

	auto text_shader{ renderer.GetShader("text") };

	auto style_hash{ Hash(style) };

	TextVertexBuildParams params;

	params.depth			  = depth;
	params.entity_id		  = entity_id;
	params.clip_rect		  = clip_rect;
	params.reveal_glyph_count = reveal_glyph_count;

	std::vector<impl::TextureVertex> text_vertices;
	std::vector<std::uint32_t> text_indices;

	BuildVertices(layout, params, text_vertices, text_indices);

	if (text_vertices.empty()) {
		return;
	}

	renderer.DrawVertices<impl::TextureVertex>(
		"text", text_shader, text_vertices, text_indices, style_hash,
		[style](impl::Renderer& renderer) {
			auto text_shader{ renderer.GetShader("text") };
			renderer.SetUniform(text_shader, "u_Weight", style.weight);
			renderer.SetUniform(text_shader, "u_Softness", style.softness);
			renderer.SetUniform(text_shader, "u_OutlineColor", style.outline_color.Normalized());
			renderer.SetUniform(text_shader, "u_OutlineWidth", style.outline_width);
			renderer.SetUniform(text_shader, "u_OutlineSoftness", style.outline_softness);
			renderer.SetUniform(text_shader, "u_GlowColor", style.glow_color.Normalized());
			renderer.SetUniform(text_shader, "u_GlowOuterWidth", style.glow_outer_width);
			renderer.SetUniform(text_shader, "u_GlowSoftness", style.glow_softness);
		}
	);
}

void TextSystem::DrawText(DrawContext& renderer, Entity entity) {
	// TODO: Pull this info from text entity.

	std::optional<Rect> clip_rect{ std::nullopt };
	std::size_t reveal_glyph_count{ std::numeric_limits<size_t>::max() };

	static impl::MsdfFontData font{
		renderer.renderer_, "assets/fonts/LiberationSans-Regular.ttf", 0, {}
	};

	TextRunStyle base{};
	base.font  = &font;
	base.color = { 1, 1, 1, 1 };
	base.scale = 32.0f;

	TextLayoutRequest request{};
	request.styled_text =
		StyledTextBuilder{ base }.Text("Hello ").Color(color::Black).Bold().Text("world").Build();

	request.box = TextBox{ .rect  = { { 0.0f, 0.0f }, { 400.0f, 100.0f } },
						   .style = {
							   .horizontal_align = HorizontalAlign::Center,
							   .vertical_align	 = VerticalAlign::Center,
							   .wrap_mode		 = WrapMode::Word,
						   } };

	// auto request = text::MakeTextRequest(
	//   "Hello world",
	//   font,
	//   {{0, 0}, {400, 100}},
	//   32.0f,
	//   {1, 1, 1, 1},
	//   { .horizontal_align = text::HorizontalAlign::kCenter,
	//     .vertical_align = text::VerticalAlign::kCenter,
	//     .wrap_mode = text::WrapMode::kWord }
	//);

	auto depth{ 0 /*GetDepth(entity)*/ };
	auto entity_id{ 1 /*entity.GetUUID()*/ };

	RenderText(renderer, request, depth, entity_id, clip_rect, reveal_glyph_count);
}

} // namespace ptgn
