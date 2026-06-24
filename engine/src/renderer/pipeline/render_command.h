#pragma once

#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"

namespace ptgn {

class Renderer;

namespace impl {

enum class RenderCommandKind : std::uint8_t {
	ColorQuads,
	ColorTriangles,
	ShapeQuads,
	TextureQuads
};

struct RenderRange {
	std::size_t first{ 0 };
	std::size_t count{ 0 };

	[[nodiscard]] std::size_t End() const {
		return first + count;
	}
};

struct RenderCommand {
	RenderCommandKind kind{ RenderCommandKind::ColorQuads };
	RenderRange range;

	MaterialState material;
	TextureId texture;
	std::optional<BlendMode> blend_mode;

	Depth depth;
	std::uint64_t sequence{ 0 };

	[[nodiscard]] bool CanMergeWith(const RenderCommand& next) const {
		return kind == next.kind && material == next.material && texture == next.texture &&
			   blend_mode == next.blend_mode && depth == next.depth &&
			   range.End() == next.range.first;
	}
};

class RenderCommands {
public:
	/// @brief Combines other render commands into this one.
	void CombineWith(RenderCommands&& other);

	[[nodiscard]] std::size_t Count() const;

	Depth GetDepth(std::size_t command_index) const;

	void Sort();

	void Draw(Renderer& renderer, std::size_t command_index);

	void Clear();

	void Add(
		const MaterialState& material, std::span<TextureQuad> primitives,
		std::optional<BlendMode> blend_mode, Depth depth, TextureId texture
	);

	void Add(
		const MaterialState& material, std::span<ShapeQuad> primitives,
		std::optional<BlendMode> blend_mode, Depth depth, TextureId
	);

	void Add(
		const MaterialState& material, std::span<ColorQuad> primitives,
		std::optional<BlendMode> blend_mode, Depth depth, TextureId
	);

	void Add(
		const MaterialState& material, std::span<ColorTriangle> primitives,
		std::optional<BlendMode> blend_mode, Depth depth, TextureId
	);

private:
	template <RenderPrimitive TPrimitive>
	[[nodiscard]] RenderRange Append(
		std::vector<TPrimitive>& dst, std::span<TPrimitive> src
	) const {
		auto first{ dst.size() };

		dst.append_range(src | std::views::as_rvalue);

		return RenderRange{ .first = first, .count = src.size() };
	}

	void Push(
		RenderCommandKind kind, RenderRange range, const MaterialState& material, TextureId texture,
		std::optional<BlendMode> blend_mode, Depth depth
	);

	std::vector<RenderCommand> commands_;

	std::vector<ColorQuad> color_quads_;
	std::vector<ColorTriangle> color_triangles_;
	std::vector<ShapeQuad> shape_quads_;
	std::vector<TextureQuad> texture_quads_;

	std::uint64_t next_sequence_{ 0 };
};

} // namespace impl

} // namespace ptgn