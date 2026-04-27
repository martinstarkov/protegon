#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/math/geometry/rect.h"
#include "renderer/resources/id.h"
#include "renderer/vertex/vertex.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_data.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_layout.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

class DrawContext;
class AssetManager;

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

struct TextVertexBuildParams {
	float depth{ 0.0f };
	int entity_id{ -1 };

	std::optional<Rect> clip_rect;

	std::size_t reveal_glyph_count{ std::numeric_limits<size_t>::max() };
	float time{ 0.0f };
};

class TextLayoutCache {
public:
	[[nodiscard]] bool TryGet(const TextLayoutKey& key, TextLayout* layout) const;
	void Put(const TextLayoutKey& key, TextLayout layout);
	void Clear();

private:
	std::unordered_map<TextLayoutKey, TextLayout> cache_{};
};

class TextSystem {
public:
	[[nodiscard]] TextLayout BuildLayout(AssetManager& asset_manager, TextLayoutRequest request);
	[[nodiscard]] TextLayout BuildLayout(
		AssetManager& asset_manager, std::string_view text, std::string_view font_key, Rect rect,
		TextLayoutStyle style, TextRunStyle run_style
	);

	[[nodiscard]] TextMeasurement Measure(AssetManager& asset_manager, TextLayoutRequest request);
	[[nodiscard]] TextMeasurement Measure(
		AssetManager& asset_manager, std::string_view text, std::string_view font_key, Rect rect,
		TextLayoutStyle style, TextRunStyle run_style
	);

	void BuildVertices(
		TextLayout& layout, TextVertexBuildParams params,
		std::vector<impl::TextureVertex>& vertices, std::vector<std::uint32_t>& local_indices,
		std::vector<impl::TextureId>& local_textures
	) const;

	[[nodiscard]] static StyledText MakePlainText(
		std::string_view text, const TextRunStyle& run_style
	);

	[[nodiscard]] static TextLayoutKey MakeKey(TextLayoutRequest request);

	void RenderText(
		AssetManager& asset_manager, DrawContext& renderer, const TextLayoutRequest& request,
		float depth, int entity_id, std::optional<Rect> clip_rect, std::size_t reveal_glyph_count
	);

	void DrawText(
		AssetManager& asset_manager, DrawContext& renderer, Entity text, std::string_view font_key
	);

private:
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

	[[nodiscard]] static Font GetFont(AssetManager& asset_manager, std::string_view font_key);

	[[nodiscard]] static std::u32string DecodeUtf8(std::string_view text);

	[[nodiscard]] static uint32_t QuantizeUnsigned(float value, float scale = 64.0f);
	[[nodiscard]] static int32_t QuantizeSigned(float value, float scale = 64.0f);

	[[nodiscard]] static std::vector<RichTextToken> Tokenize(
		AssetManager& asset_manager, StyledText& styled_text, float global_shrink
	);
	[[nodiscard]] static float MeasureLineHeight(AssetManager& asset_manager, TextRunStyle& style);
	[[nodiscard]] static float MeasureTokenWidth(
		AssetManager& asset_manager, RichTextToken& token, StyledText& styled_text,
		float global_shrink
	);
	[[nodiscard]] static bool FitsInBox(TextLayout& layout, Rect box);
	[[nodiscard]] static CandidateLayout BuildSinglePassLayout(
		AssetManager& asset_manager, StyledText& styled_text, TextBox box, float global_shrink
	);
	[[nodiscard]] static float FindBestShrinkScale(
		AssetManager& asset_manager, StyledText& styled_text, TextBox box
	);

	static void ApplyVerticalAlignment(TextBox box, TextLayout* layout);
	static void ApplyEllipsisForMaxLines(
		AssetManager& asset_manager, StyledText& styled_text, TextBox box, float global_shrink,
		TextLayout* layout
	);
	static void ApplyClipVisibility(Rect clip_rect, TextLayout* layout);

	[[nodiscard]] static std::optional<ResolvedGlyph> ResolveGlyph(
		AssetManager& asset_manager, TextRun& run, uint32_t codepoint, uint32_t next_codepoint,
		size_t source_run_index, size_t source_codepoint_index, float global_shrink
	);

	static void EmitGlyphQuad(
		GlyphInstance& glyph, const TextVertexBuildParams& params,
		std::vector<impl::TextureVertex>& vertices, std::vector<std::uint32_t>& local_indices
	);

	TextLayoutCache cache_;
};

} // namespace ptgn