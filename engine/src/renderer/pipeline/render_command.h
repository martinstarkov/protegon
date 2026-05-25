#pragma once

#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/vertex.h"
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

	ShaderId shader;
	TextureId texture;
	std::optional<BlendMode> blend_mode;

	float depth{ 0.0f };
	std::uint64_t sequence{ 0 };

	[[nodiscard]] bool CanMergeWith(const RenderCommand& next) const {
		return kind == next.kind && shader == next.shader && texture == next.texture &&
			   blend_mode == next.blend_mode && depth == next.depth &&
			   range.End() == next.range.first;
	}
};

class RenderCommands {
public:
	/// @brief Combines other render commands into this one.
	void CombineWith(RenderCommands&& other);

	[[nodiscard]] std::size_t Count() const;

	float GetDepth(std::size_t command_index) const;

	void Sort();

	void Draw(Renderer& renderer, std::size_t command_index);

	void Clear();

	void Add(
		ShaderId shader, std::span<RenderQuad<TextureVertex>> primitives,
		std::optional<BlendMode> blend_mode, float depth, TextureId texture
	);

	void Add(
		ShaderId shader, std::span<RenderQuad<ShapeVertex>> primitives,
		std::optional<BlendMode> blend_mode, float depth, TextureId
	);

	void Add(
		ShaderId shader, std::span<RenderQuad<ColorVertex>> primitives,
		std::optional<BlendMode> blend_mode, float depth, TextureId
	);

	void Add(
		ShaderId shader, std::span<RenderTriangle<ColorVertex>> primitives,
		std::optional<BlendMode> blend_mode, float depth, TextureId
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
		RenderCommandKind kind, RenderRange range, ShaderId shader, TextureId texture,
		std::optional<BlendMode> blend_mode, float depth
	);

	std::vector<RenderCommand> commands_;

	std::vector<RenderQuad<ColorVertex>> color_quads_;
	std::vector<RenderTriangle<ColorVertex>> color_triangles_;
	std::vector<RenderQuad<ShapeVertex>> shape_quads_;
	std::vector<RenderQuad<TextureVertex>> texture_quads_;

	std::uint64_t next_sequence_{ 0 };
};

} // namespace impl

} // namespace ptgn