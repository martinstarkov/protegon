#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/resources/id.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

class DrawContext;
class AssetManager;

struct TextLayoutStyle {
	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::None };
	OverflowMode overflow_mode{ OverflowMode::Overflow };

	bool collapse_spaces{ false };
	bool justify_last_line{ false };
	bool allow_word_break_in_overflow{ true };

	std::size_t max_lines{ 0 };

	float min_shrink_scale{ 0.25f };
	float max_shrink_scale{ 1.0f };
};

struct TextBox {
	Rect rect;
	TextLayoutStyle style;
};

struct LineLayout {
	std::size_t glyph_begin{ 0 };
	std::size_t glyph_end{ 0 };

	V2_float size;
	float baseline_y{ 0.0f };

	std::size_t justify_space_count{ 0 };
	float justify_extra_per_space{ 0.0f };
	bool ends_with_explicit_newline{ false };
};

struct TextMeasurement {
	V2_float size;
	float first_line_height{ 0.0f };
	float max_line_width{ 0.0f };
	std::size_t line_count{ 0 };
	bool truncated{ false };
	float used_shrink_scale{ 1.0f };
};

struct TextBatchStyle {
	impl::TextureId texture{ 0 };
	DistanceFieldStyle sdf;

	bool operator==(const TextBatchStyle&) const = default;
};

enum class TextDecorationType : std::uint8_t {
	Underline,
	Strikethrough,
};

struct TextDecoration {
	TextDecorationType type{ TextDecorationType::Underline };
	Rect rect;
	Color color{ color::White };

	std::size_t source_run_index{ 0 };
	std::size_t line_index{ 0 };

	bool visible{ true };
};

enum class TextClipMode : std::uint8_t {
	None,
	ClipFullyContained,
	ClipFullyOutside,
};

struct TextLayout {
	std::vector<GlyphInstance> glyphs;
	std::vector<TextDecoration> decorations;
	std::vector<LineLayout> lines;

	// One style per StyledText run. GlyphInstance::source_run_index indexes this.
	std::vector<TextBatchStyle> batch_styles;

	V2_float measured_size;
	V2_float content_offset;
	float used_shrink_scale{ 1.0f };

	bool clipped{ false };
	bool ellipsized{ false };
	bool truncated_by_max_lines{ false };

	std::optional<Rect> clip_rect;
	TextClipMode clip_mode{ TextClipMode::None };
	Rect local_box;

	std::size_t hash{ 0 };
};

struct TextPageOptions {
	std::string split_end{ "..." };
	std::string split_begin{ "..." };

	std::size_t max_lines_per_page{ 0 };

	bool add_split_markers{ true };
};

struct TextPage {
	StyledText styled_text;
	TextMeasurement measurement;
	std::size_t glyph_count{ 0 };
};

struct TextPaginationResult {
	std::vector<TextPage> pages;
};

namespace impl {

struct TextDrawBatch {
	TextBatchStyle style;
	bool decoration{ false };
	std::vector<impl::TextureQuad> quads;
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

struct CandidateLayout {
	TextLayout layout;
	float used_shrink_scale{ 1.0f };
};

V2_float GetTextOriginPoint(Rect rect, Origin origin);

void UpdateLayout(
	Entity entity, AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
);

[[nodiscard]] TextLayout BuildLayout(
	AssetManager& asset_manager, StyledText styled_text, const TextBox& box
);

[[nodiscard]] TextMeasurement Measure(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
);

void BuildVertices(
	const TextLayout& layout, float depth, int entity_id, std::optional<Rect> clip_rect,
	TextClipMode clip_mode, std::size_t reveal_glyph_count, float time,
	std::vector<TextDrawBatch>& batches
);

void DrawText(AssetManager& asset_manager, DrawContext& ctx, Entity text);

[[nodiscard]] Font GetFont(AssetManager& asset_manager, std::string_view font_key);

[[nodiscard]] std::u32string DecodeUtf8(std::string_view text);

[[nodiscard]] std::vector<RichTextToken> Tokenize(
	AssetManager& asset_manager, StyledText& styled_text, bool collapse_spaces, float global_shrink
);
[[nodiscard]] float MeasureLineHeight(AssetManager& asset_manager, const TextRunStyle& style);
[[nodiscard]] float MeasureTokenWidth(
	AssetManager& asset_manager, RichTextToken& token, StyledText& styled_text, float global_shrink
);
[[nodiscard]] bool FitsInBox(const TextLayout& layout, Rect box);
[[nodiscard]] CandidateLayout BuildSinglePassLayout(
	AssetManager& asset_manager, StyledText& styled_text, const TextBox& box, float global_shrink
);
[[nodiscard]] float FindBestShrinkScale(
	AssetManager& asset_manager, StyledText& styled_text, const TextBox& box
);

void ApplyVerticalAlignment(const TextBox& box, TextLayout* layout);
void ApplyEllipsisOverflow(
	AssetManager& asset_manager, StyledText& styled_text, const TextBox& box, float global_shrink,
	TextLayout* layout
);
void ApplyMaxLines(const TextBox& box, TextLayout* layout);
void ApplyClipVisibility(Rect clip_rect, TextLayout* layout);

[[nodiscard]] std::optional<ResolvedGlyph> ResolveGlyph(
	AssetManager& asset_manager, const TextRun& run, uint32_t codepoint, uint32_t next_codepoint,
	size_t source_run_index, size_t source_codepoint_index, float global_shrink
);

void EmitGlyphQuad(
	const GlyphInstance& glyph, float depth, int entity_id, float time,
	std::vector<impl::TextureQuad>& quads
);

struct TextReveal {
	std::size_t glyph_count{ std::numeric_limits<std::size_t>::max() };
};

struct TextClip {
	std::optional<Rect> rect;
	TextClipMode mode{ TextClipMode::ClipFullyOutside };
};

} // namespace impl

} // namespace ptgn