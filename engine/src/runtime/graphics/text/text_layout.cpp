#include "runtime/graphics/text/text_layout.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

namespace {

[[nodiscard]] bool IsWhitespace(std::uint32_t cp) {
	return cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r';
}

[[nodiscard]] std::uint32_t GetNextCodepoint(std::u32string_view text, std::size_t index) {
	if (index + 1 < text.size()) {
		return text[index + 1];
	}
	return 0;
}

std::uint32_t QuantizeUnsigned(float value, float scale = 64.0f) {
	return static_cast<std::uint32_t>(std::lround(value * scale));
}

std::int32_t QuantizeSigned(float value, float scale = 64.0f) {
	return static_cast<int32_t>(std::lround(value * scale));
}

std::optional<Rect> GetVisibleGlyphBounds(const TextLayout& layout) {
	bool found{ false };
	V2_float min;
	V2_float max;

	for (const auto& glyph : layout.glyphs) {
		if (!glyph.visible) {
			continue;
		}

		V2_float glyph_min{ glyph.position + glyph.plane.GetMin() };
		V2_float glyph_max{ glyph.position + glyph.plane.GetMax() };

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

} // namespace

namespace impl {

void UpdateLayout(
	Entity entity, AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
) {
	std::size_t hash{ ptgn::Hash(
		Hash(styled_text), QuantizeUnsigned(box.rect.GetSize().x),
		QuantizeUnsigned(box.rect.GetSize().y), QuantizeUnsigned(box.style.min_shrink_scale),
		QuantizeUnsigned(box.style.max_shrink_scale),
		std::to_underlying(box.style.horizontal_align),
		std::to_underlying(box.style.vertical_align), std::to_underlying(box.style.wrap_mode),
		std::to_underlying(box.style.overflow_mode), box.style.collapse_spaces,
		box.style.justify_last_line, box.style.allow_word_break_in_overflow, box.style.max_lines,
		box.style.ellipsis_on_max_lines
	) };

	if (auto cached{ entity.TryGet<TextLayout>() }) {
		if (cached->hash == hash) {
			return;
		}
	}

	auto layout{ BuildLayout(asset_manager, styled_text, box) };

	layout.hash = hash;

	entity.Add<TextLayout>(layout);
}

TextLayout BuildLayout(AssetManager& asset_manager, StyledText styled_text, const TextBox& box) {
	float shrink{ 1.0f };
	if (box.style.overflow_mode == OverflowMode::ShrinkToFit) {
		shrink = FindBestShrinkScale(asset_manager, styled_text, box);
	}

	auto candidate{ BuildSinglePassLayout(asset_manager, styled_text, box, shrink) };
	TextLayout layout{ std::move(candidate.layout) };
	layout.used_shrink_scale = candidate.used_shrink_scale;

	if (box.style.max_lines > 0 && layout.lines.size() > box.style.max_lines) {
		layout.truncated_by_max_lines = true;
		if (box.style.ellipsis_on_max_lines) {
			ApplyEllipsisForMaxLines(asset_manager, styled_text, box, shrink, &layout);
		} else {
			auto last_line_index{ box.style.max_lines - 1 };
			auto hide_from{ layout.lines[last_line_index].glyph_end };
			for (auto i{ hide_from }; i < layout.glyphs.size(); ++i) {
				layout.glyphs[i].visible = false;
			}
			layout.lines.resize(box.style.max_lines);
			layout.measured_size.y = static_cast<float>(layout.lines.size()) *
									 (layout.lines.empty() ? 0.0f : layout.lines.front().size.y);
		}
	}

	if (box.style.overflow_mode == OverflowMode::Clip) {
		ApplyClipVisibility(box.rect, &layout);
		layout.clipped = true;
	}

	ApplyVerticalAlignment(box, &layout);

	if (auto bounds{ GetVisibleGlyphBounds(layout) }) {
		V2_float visual_center{ (bounds->GetMin() + bounds->GetMax()) * 0.5f };

		for (auto& glyph : layout.glyphs) {
			glyph.position -= visual_center;
		}
	}

	return layout;
}

TextMeasurement Measure(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
) {
	TextLayout layout{ BuildLayout(asset_manager, styled_text, box) };

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

void BuildVertices(
	const TextLayout& layout, Transform transform, float depth, int entity_id,
	std::optional<Rect> clip_rect, std::size_t reveal_glyph_count, float time,
	std::vector<impl::TextureVertex>& vertices, std::vector<std::uint32_t>& local_indices,
	std::vector<impl::TextureId>& local_textures
) {
	for (GlyphInstance glyph : layout.glyphs) {
		if (!glyph.visible) {
			continue;
		}
		if (glyph.visible_order >= reveal_glyph_count) {
			continue;
		}

		if (clip_rect.has_value()) {
			V2_float gmin{ glyph.position + glyph.plane.GetMin() };
			V2_float gmax{ glyph.position + glyph.plane.GetMax() };

			if (gmax.x <= clip_rect->GetMin().x || gmin.x >= clip_rect->GetMax().x ||
				gmax.y <= clip_rect->GetMin().y || gmin.y >= clip_rect->GetMax().y) {
				continue;
			}
		}

		if (auto it{ std::ranges::find(local_textures, glyph.texture) };
			it == local_textures.end()) {
			local_textures.push_back(glyph.texture);
			glyph.texture_index = static_cast<std::uint32_t>(local_textures.size() - 1);
		} else {
			glyph.texture_index =
				static_cast<std::uint32_t>(std::distance(local_textures.begin(), it));
		}

		EmitGlyphQuad(glyph, transform, depth, entity_id, time, vertices, local_indices);
	}
}

Font GetFont(AssetManager& asset_manager, std::string_view font_key) {
	return asset_manager.Get<Font>(font_key);
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

std::vector<RichTextToken> Tokenize(
	AssetManager& asset_manager, StyledText& styled_text, float global_shrink
) {
	std::vector<RichTextToken> tokens{};

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
				token.width = MeasureTokenWidth(asset_manager, token, styled_text, global_shrink);
				tokens.push_back(std::move(token));
				continue;
			}

			if (cp == U'\t') {
				RichTextToken token;
				token.type		= RichTextToken::Type::Tab;
				token.run_index = run_index;
				token.text.push_back(U'\t');
				token.width = MeasureTokenWidth(asset_manager, token, styled_text, global_shrink);
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
			token.width		= MeasureTokenWidth(asset_manager, token, styled_text, global_shrink);
			tokens.push_back(std::move(token));
		}
	}

	return tokens;
}

float MeasureLineHeight(AssetManager& asset_manager, const TextRunStyle& style) {
	auto font{ GetFont(asset_manager, style.font) };
	impl::FontMetrics metrics{ font.GetFontData().metrics };
	return (metrics.line_height + style.line_spacing) * style.scale;
}

float MeasureTokenWidth(
	AssetManager& asset_manager, RichTextToken& token, StyledText& styled_text, float global_shrink
) {
	if (token.run_index >= styled_text.runs.size()) {
		return 0.0f;
	}

	const TextRun& run{ styled_text.runs[token.run_index] };
	auto font{ GetFont(asset_manager, run.style.font) };

	float scale{ run.style.scale * global_shrink };

	if (token.type == RichTextToken::Type::Tab) {
		float space_adv{ font.GetAdvance(U' ', 0) };
		return (space_adv + run.style.kerning + run.style.tracking) * scale * 4.0f;
	}

	float width{ 0.0f };
	for (auto i{ 0uz }; i < token.text.size(); ++i) {
		std::uint32_t cp{ token.text[i] };
		std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
		width += (font.GetAdvance(cp, next_cp) + run.style.kerning + run.style.tracking) * scale;
	}

	return width;
}

bool FitsInBox(const TextLayout& layout, Rect box) {
	return layout.measured_size.x <= box.GetSize().x && layout.measured_size.y <= box.GetSize().y;
}

std::optional<ResolvedGlyph> ResolveGlyph(
	AssetManager& asset_manager, const TextRun& run, std::uint32_t codepoint,
	std::uint32_t next_codepoint, std::size_t source_run_index, std::size_t source_codepoint_index,
	float global_shrink
) {
	auto font{ GetFont(asset_manager, run.style.font) };

	std::optional<impl::GlyphMetrics> metrics{ font.GetGlyph(codepoint) };
	if (!metrics.has_value()) {
		metrics = font.GetGlyph(U'?');
	}
	if (!metrics.has_value()) {
		return std::nullopt;
	}

	ResolvedGlyph resolved;
	resolved.codepoint				= codepoint;
	resolved.metrics				= *metrics;
	resolved.source_run_index		= source_run_index;
	resolved.source_codepoint_index = source_codepoint_index;
	resolved.texture				= font.GetAtlasTexture();

	resolved.render_style.color			   = run.style.color;
	resolved.render_style.effect.type	   = run.style.effect.type;
	resolved.render_style.effect.amplitude = run.style.effect.amplitude;
	resolved.render_style.effect.frequency = run.style.effect.frequency;
	resolved.render_style.effect.speed	   = run.style.effect.speed;
	resolved.render_style.effect.phase	   = run.style.effect.phase;

	resolved.metrics.plane.GetMin() *= run.style.scale * global_shrink;
	resolved.metrics.plane.GetMax() *= run.style.scale * global_shrink;
	resolved.metrics.advance =
		(font.GetAdvance(codepoint, next_codepoint) + run.style.kerning + run.style.tracking) *
		(run.style.scale * global_shrink);

	return resolved;
}

CandidateLayout BuildSinglePassLayout(
	AssetManager& asset_manager, StyledText& styled_text, const TextBox& box, float global_shrink
) {
	std::vector<RichTextToken> tokens{ Tokenize(asset_manager, styled_text, global_shrink) };

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
			glyph.position.y	+= box.rect.GetMin().y;
			glyph.line_index	 = layout.lines.size();
			glyph.visible_order	 = visible_order++;

			if (box.style.horizontal_align == HorizontalAlign::Justify &&
				line.justify_extra_per_space > 0.0f &&
				(glyph.codepoint == U' ' || glyph.codepoint == U'\t')) {
				justify_extra += line.justify_extra_per_space;
			}
		}

		layout.glyphs.append_range(current_line_glyphs);
		layout.lines.push_back(line);

		layout.measured_size.x	= std::max(layout.measured_size.x, line.size.x);
		layout.measured_size.y += line.size.y;

		current_line_glyphs.clear();
		current_line_size  = {};
		y				  += line.size.y;
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

		const auto& run{ styled_text.runs[token.run_index] };
		float line_h{ MeasureLineHeight(asset_manager, run.style) * global_shrink };
		current_line_size.y = std::max(current_line_size.y, line_h);

		if (token.type == RichTextToken::Type::Space || token.type == RichTextToken::Type::Tab) {
			if (std::optional<ResolvedGlyph> space_glyph{ ResolveGlyph(
					asset_manager, run, token.type == RichTextToken::Type::Space ? U' ' : U'\t', 0,
					token.run_index, 0, global_shrink
				) };
				space_glyph.has_value()) {
				GlyphInstance glyph;
				glyph.codepoint				 = space_glyph->codepoint;
				glyph.position				 = { current_line_size.x, y };
				glyph.plane					 = space_glyph->metrics.plane;
				glyph.uv					 = space_glyph->metrics.uv;
				glyph.source_run_index		 = token.run_index;
				glyph.source_codepoint_index = 0;
				glyph.render_style			 = space_glyph->render_style;
				glyph.texture				 = space_glyph->texture;
				current_line_glyphs.push_back(glyph);
			}

			current_line_size.x += token.width;
			continue;
		}

		if (token.type == RichTextToken::Type::Word && token.width > box.rect.GetSize().x &&
			box.style.wrap_mode == WrapMode::Character && box.style.allow_word_break_in_overflow) {
			for (auto i{ 0uz }; i < token.text.size(); ++i) {
				std::uint32_t cp{ token.text[i] };
				std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
				std::optional<ResolvedGlyph> resolved{
					ResolveGlyph(asset_manager, run, cp, next_cp, token.run_index, i, global_shrink)
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
				glyph.source_run_index		 = resolved->source_run_index;
				glyph.source_codepoint_index = resolved->source_codepoint_index;
				glyph.render_style			 = resolved->render_style;
				glyph.texture				 = resolved->texture;
				current_line_glyphs.push_back(glyph);

				current_line_size.x += resolved->metrics.advance;
			}
			continue;
		}

		float x{ current_line_size.x };
		for (auto i{ 0uz }; i < token.text.size(); ++i) {
			std::uint32_t cp{ token.text[i] };
			std::uint32_t next_cp{ GetNextCodepoint(token.text, i) };
			std::optional<ResolvedGlyph> resolved{
				ResolveGlyph(asset_manager, run, cp, next_cp, token.run_index, i, global_shrink)
			};
			if (!resolved.has_value()) {
				continue;
			}

			GlyphInstance glyph;
			glyph.codepoint				 = resolved->codepoint;
			glyph.position				 = { x, y };
			glyph.plane					 = resolved->metrics.plane;
			glyph.uv					 = resolved->metrics.uv;
			glyph.source_run_index		 = resolved->source_run_index;
			glyph.source_codepoint_index = resolved->source_codepoint_index;
			glyph.render_style			 = resolved->render_style;
			glyph.texture				 = resolved->texture;
			current_line_glyphs.push_back(glyph);

			x += resolved->metrics.advance;
		}

		current_line_size.x = x;
	}

	flush_line(false);

	if (!styled_text.runs.empty()) {
		layout.batch_style = styled_text.runs.front().style.sdf;

		if (HasFlag(styled_text.runs.front().style.flags, FontStyle::Bold)) {
			layout.batch_style.weight += styled_text.runs.front().style.fake_bold_weight;
		}
	}

	CandidateLayout result;
	result.layout			 = std::move(layout);
	result.used_shrink_scale = global_shrink;
	return result;
}

float FindBestShrinkScale(
	AssetManager& asset_manager, StyledText& styled_text, const TextBox& box
) {
	float lo{ box.style.min_shrink_scale };
	float hi{ box.style.max_shrink_scale };
	float best{ lo };

	for (int i{ 0 }; i < 16; ++i) {
		float mid{ 0.5f * (lo + hi) };
		CandidateLayout candidate{ BuildSinglePassLayout(asset_manager, styled_text, box, mid) };
		if (FitsInBox(candidate.layout, box.rect)) {
			best = mid;
			lo	 = mid;
		} else {
			hi = mid;
		}
	}

	return best;
}

void ApplyVerticalAlignment(const TextBox& box, TextLayout* layout) {
	if (!layout) {
		return;
	}

	float offset_y{ 0.0f };

	switch (box.style.vertical_align) {
		using enum VerticalAlign;
		case Top:	 offset_y = 0.0f; break;
		case Center: offset_y = (box.rect.GetSize().y - layout->measured_size.y) * 0.5f; break;
		case Bottom: offset_y = box.rect.GetSize().y - layout->measured_size.y; break;
	}

	for (GlyphInstance& glyph : layout->glyphs) {
		glyph.position.y += offset_y;
	}

	layout->content_offset = { 0.0f, offset_y };
}

void ApplyEllipsisForMaxLines(
	AssetManager& asset_manager, StyledText& styled_text, const TextBox& box, float global_shrink,
	TextLayout* layout
) {
	if (!layout) {
		return;
	}

	if (box.style.max_lines == 0 || layout->lines.size() <= box.style.max_lines) {
		return;
	}

	auto keep_lines{ box.style.max_lines };
	auto last_visible_line_index{ keep_lines - 1 };
	const LineLayout& last_line{ layout->lines[last_visible_line_index] };

	auto hide_from{ last_line.glyph_end };
	for (auto i{ hide_from }; i < layout->glyphs.size(); ++i) {
		layout->glyphs[i].visible = false;
	}

	const TextRun* source_run{ nullptr };
	if (last_line.glyph_begin < layout->glyphs.size()) {
		const auto& anchor{ layout->glyphs[last_line.glyph_begin] };
		if (anchor.source_run_index < styled_text.runs.size()) {
			source_run = &styled_text.runs[anchor.source_run_index];
		}
	}

	if (!source_run) {
		layout->lines.resize(keep_lines);
		layout->ellipsized			   = true;
		layout->truncated_by_max_lines = true;
		return;
	}

	auto font{ GetFont(asset_manager, source_run->style.font) };

	std::u32string dots{ U"..." };
	float dots_width{ 0.0f };
	for (auto i{ 0uz }; i < dots.size(); ++i) {
		std::uint32_t cp{ dots[i] };
		std::uint32_t next_cp{ GetNextCodepoint(dots, i) };
		dots_width += (font.GetAdvance(cp, next_cp) + source_run->style.kerning +
					   source_run->style.tracking) *
					  (source_run->style.scale * global_shrink);
	}

	auto cutoff{ last_line.glyph_end };
	float usable_x{ box.rect.GetMin().x + box.rect.GetSize().x - dots_width };

	for (auto i{ last_line.glyph_begin }; i < last_line.glyph_end; ++i) {
		auto& glyph{ layout->glyphs[i] };
		float right{ glyph.position.x + glyph.plane.GetMax().x };
		if (right > usable_x) {
			cutoff = i;
			break;
		}
	}

	for (auto i{ cutoff }; i < last_line.glyph_end; ++i) {
		layout->glyphs[i].visible = false;
	}

	float start_x{ box.rect.GetMin().x };
	if (cutoff > last_line.glyph_begin) {
		GlyphInstance& prev{ layout->glyphs[cutoff - 1] };
		start_x = prev.position.x + prev.plane.GetMax().x;
	}

	float y{ layout->glyphs[last_line.glyph_begin].position.y };

	for (auto i{ 0uz }; i < dots.size(); ++i) {
		std::uint32_t cp{ dots[i] };
		std::uint32_t next_cp{ GetNextCodepoint(dots, i) };
		std::optional<ResolvedGlyph> resolved{ ResolveGlyph(
			asset_manager, *source_run, cp, next_cp,
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
		glyph.source_run_index		 = resolved->source_run_index;
		glyph.source_codepoint_index = resolved->source_codepoint_index;
		glyph.render_style			 = resolved->render_style;
		glyph.line_index			 = last_visible_line_index;
		glyph.visible_order			 = layout->glyphs.size();
		glyph.texture				 = resolved->texture;
		layout->glyphs.push_back(glyph);

		start_x += resolved->metrics.advance;
	}

	layout->lines.resize(keep_lines);
	layout->ellipsized			   = true;
	layout->truncated_by_max_lines = true;
	layout->measured_size.y =
		static_cast<float>(layout->lines.size()) * layout->lines.front().size.y;
}

void ApplyClipVisibility(Rect clip_rect, TextLayout* layout) {
	if (!layout) {
		return;
	}

	for (auto& glyph : layout->glyphs) {
		V2_float gmin{ glyph.position + glyph.plane.GetMin() };
		V2_float gmax{ glyph.position + glyph.plane.GetMax() };

		if (gmax.x <= clip_rect.GetMin().x || gmin.x >= clip_rect.GetMax().x ||
			gmax.y <= clip_rect.GetMin().y || gmin.y >= clip_rect.GetMax().y) {
			glyph.visible = false;
		}
	}
}

void EmitGlyphQuad(
	const GlyphInstance& glyph, Transform transform, float depth, int entity_id, float time,
	std::vector<impl::TextureVertex>& vertices, std::vector<std::uint32_t>& local_indices
) {
	auto effect_offset{ glyph.GetEffectOffset(time) };

	auto quad_min{ glyph.position + glyph.plane.GetMin() + effect_offset };
	auto quad_max{ glyph.position + glyph.plane.GetMax() + effect_offset };

	if (float scale{ glyph.GetEffectScale(time) }; !NearlyEqual(scale, 1.0f)) {
		auto center{ (quad_min + quad_max) * 0.5f };
		quad_min = center + (quad_min - center) * scale;
		quad_max = center + (quad_max - center) * scale;
	}

	std::array positions{ quad_min, V2_float{ quad_max.x, quad_min.y }, quad_max,
						  V2_float{ quad_min.x, quad_max.y } };

	for (auto& position : positions) {
		position = transform.Apply(position);
	}

	auto color_n{ glyph.render_style.color.Normalized() };

	vertices.emplace_back(
		positions[0], depth, color_n, glyph.uv.GetMin(), static_cast<float>(glyph.texture_index),
		entity_id
	);

	vertices.emplace_back(
		positions[1], depth, color_n, V2_float{ glyph.uv.GetMax().x, glyph.uv.GetMin().y },
		static_cast<float>(glyph.texture_index), entity_id
	);

	vertices.emplace_back(
		positions[2], depth, color_n, glyph.uv.GetMax(), static_cast<float>(glyph.texture_index),
		entity_id
	);

	vertices.emplace_back(
		positions[3], depth, color_n, V2_float{ glyph.uv.GetMin().x, glyph.uv.GetMax().y },
		static_cast<float>(glyph.texture_index), entity_id
	);

	local_indices.emplace_back(0);
	local_indices.emplace_back(1);
	local_indices.emplace_back(2);
	local_indices.emplace_back(2);
	local_indices.emplace_back(3);
	local_indices.emplace_back(0);
}

void DrawText(AssetManager& asset_manager, DrawContext& ctx, Entity entity) {
	// TODO: Pull this info from text entity.

	std::optional<Rect> clip_rect{ std::nullopt };
	constexpr std::size_t reveal_glyph_count{ std::numeric_limits<size_t>::max() };

	auto font{ entity.Get<Font>() };

	auto font_key{ font.GetEntity().Get<AssetName>() };

	const auto& font_data{ font.GetFontData() };

	PTGN_ASSERT(font_data.metrics.em_size > 0, "Invalid font em size");
	PTGN_ASSERT(font_data.metrics.pixel_range > 0, "Invalid font pixel range");

	StyledText styled_text;
	TextRun run;
	run.text				  = std::string{ "HELPME" };
	run.style.sdf.pixel_range = font_data.metrics.pixel_range;
	run.style.font			  = font_key.value;
	// TODO: Get font size from text entity.
	run.style.scale = 48.0f;
	run.style.color = color::Black;
	styled_text.runs.push_back(std::move(run));

	auto transform{ GetDrawTransform(entity) };

	V2_float text_size{ 400, 100 };

	TextBox box{ .rect{ text_size },
				 .style{ .horizontal_align = HorizontalAlign::Center,
						 .vertical_align   = VerticalAlign::Center,
						 .wrap_mode		   = WrapMode::Word } };

	UpdateLayout(entity, asset_manager, styled_text, box);
	const TextLayout& layout = entity.Get<TextLayout>();

	const DistanceFieldStyle& style{ layout.batch_style };

	auto style_hash{ Hash(style) };

	auto depth{ GetDepth(entity) };
	auto entity_id{ entity.GetUUID() };
	auto time{ 0.0f };

	std::vector<impl::TextureVertex> text_vertices;
	std::vector<impl::TextureId> text_textures;
	std::vector<std::uint32_t> local_indices;

	BuildVertices(
		layout, transform, depth, entity_id, clip_rect, reveal_glyph_count, time, text_vertices,
		local_indices, text_textures
	);

	if (text_vertices.empty()) {
		return;
	}

	// TODO: Fix.
	/*
	auto text_shader{ renderer.GetShader("text") };

	renderer.SetBlendMode(BlendMode::Blend);

	renderer.DrawTexturedQuads<impl::TextureVertex>(
		"text", text_shader, text_vertices, local_indices, text_textures, style_hash,
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
			PTGN_ASSERT(style.pixel_range > 0.0f, "Invalid font pixel range");
			renderer.SetUniform(text_shader, "u_PixelRange", style.pixel_range);
		}
	);
	*/
}

} // namespace impl

} // namespace ptgn
