#include "renderer/pipeline/render_command.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"

namespace ptgn::impl {

void RenderCommands::CombineWith(RenderCommands&& other) {
	auto color_quad_offset{ color_quads_.size() };
	auto color_triangle_offset{ color_triangles_.size() };
	auto shape_quad_offset{ shape_quads_.size() };
	auto texture_quad_offset{ texture_quads_.size() };

	commands_.reserve(commands_.size() + other.commands_.size());

	color_quads_.append_range(other.color_quads_ | std::views::as_rvalue);
	color_triangles_.append_range(other.color_triangles_ | std::views::as_rvalue);
	shape_quads_.append_range(other.shape_quads_ | std::views::as_rvalue);
	texture_quads_.append_range(other.texture_quads_ | std::views::as_rvalue);

	for (auto& command : other.commands_) {
		switch (command.kind) {
			using enum RenderCommandKind;
			case ColorQuads:	 command.range.first += color_quad_offset; break;
			case ColorTriangles: command.range.first += color_triangle_offset; break;
			case ShapeQuads:	 command.range.first += shape_quad_offset; break;
			case TextureQuads:	 command.range.first += texture_quad_offset; break;
			default:			 PTGN_ERROR("Unknown RenderCommandKind: ", std::to_underlying(command.kind));
		}

		commands_.emplace_back(std::move(command));
	}
}

std::size_t RenderCommands::Count() const {
	return commands_.size();
}

float RenderCommands::GetDepth(std::size_t command_index) const {
	PTGN_ASSERT(command_index < commands_.size(), "Command index outside of render command range");
	return commands_[command_index].depth;
}

void RenderCommands::Sort() {
	std::ranges::stable_sort(commands_, [](const auto& a, const auto& b) {
		if (a.depth != b.depth) {
			return a.depth < b.depth;
		}

		return a.sequence < b.sequence;
	});
}

void RenderCommands::Draw(Renderer& renderer, std::size_t command_index) const {
	const auto& command{ commands_[command_index] };

	auto prev_blend_mode{ renderer.GetBlendMode() };

	if (command.blend_mode.has_value()) {
		renderer.SetBlendMode(*command.blend_mode);
	}

	renderer.SetShader(command.shader);

	switch (command.kind) {
		case RenderCommandKind::TextureQuads: {
			renderer.SetCurrentPipeline("texture");

			std::span quads{ texture_quads_.data() + command.range.first, command.range.count };

			std::span textures{ &command.texture, 1 };

			renderer.DrawQuads<TextureVertex>(quads, textures);

			break;
		}

		case RenderCommandKind::ShapeQuads: {
			renderer.SetCurrentPipeline("shape");

			std::span quads{ shape_quads_.data() + command.range.first, command.range.count };

			renderer.DrawQuads<ShapeVertex, NoTextureIndexAccessor>(quads);

			break;
		}

		case RenderCommandKind::ColorQuads: {
			renderer.SetCurrentPipeline("color");

			std::span quads{ color_quads_.data() + command.range.first, command.range.count };

			renderer.DrawQuads<ColorVertex, NoTextureIndexAccessor>(quads);

			break;
		}

		case RenderCommandKind::ColorTriangles: {
			renderer.SetCurrentPipeline("color");

			std::span triangles{ color_triangles_.data() + command.range.first,
								 command.range.count };

			renderer.DrawTriangles<ColorVertex, NoTextureIndexAccessor>(triangles);

			break;
		}

		default: PTGN_ERROR("Unknown RenderCommandKind: ", std::to_underlying(command.kind));
	}

	renderer.SetBlendMode(prev_blend_mode);
}

void RenderCommands::Clear() {
	commands_.clear();

	color_quads_.clear();
	color_triangles_.clear();
	shape_quads_.clear();
	texture_quads_.clear();

	next_sequence_ = 0;
}

void RenderCommands::AddColorQuads(
	ShaderId shader, std::span<const RenderQuad<ColorVertex>> quads,
	std::optional<BlendMode> blend_mode, float depth
) {
	auto range{ Append(color_quads_, quads) };
	Push(RenderCommandKind::ColorQuads, range, shader, TextureId{}, blend_mode, depth);
}

void RenderCommands::AddColorTriangles(
	ShaderId shader, std::span<const RenderTriangle<ColorVertex>> triangles,
	std::optional<BlendMode> blend_mode, float depth
) {
	auto range{ Append(color_triangles_, triangles) };
	Push(RenderCommandKind::ColorTriangles, range, shader, TextureId{}, blend_mode, depth);
}

void RenderCommands::AddShapeQuads(
	ShaderId shader, std::span<const RenderQuad<ShapeVertex>> quads,
	std::optional<BlendMode> blend_mode, float depth
) {
	auto range{ Append(shape_quads_, quads) };
	Push(RenderCommandKind::ShapeQuads, range, shader, TextureId{}, blend_mode, depth);
}

void RenderCommands::AddTextureQuads(
	ShaderId shader, TextureId texture, std::span<const RenderQuad<TextureVertex>> quads,
	std::optional<BlendMode> blend_mode, float depth
) {
	auto range{ Append(texture_quads_, quads) };
	Push(RenderCommandKind::TextureQuads, range, shader, texture, blend_mode, depth);
}

void RenderCommands::Push(
	RenderCommandKind kind, RenderRange range, ShaderId shader, TextureId texture,
	std::optional<BlendMode> blend_mode, float depth
) {
	if (range.count == 0) {
		return;
	}

	RenderCommand command{ .kind	   = kind,
						   .range	   = range,
						   .shader	   = shader,
						   .texture	   = texture,
						   .blend_mode = blend_mode,
						   .depth	   = depth,
						   .sequence   = next_sequence_ };

	if (!commands_.empty()) {
		auto& previous{ commands_.back() };

		if (previous.CanMergeWith(command)) {
			previous.range.count += range.count;
			return;
		}
	}

	++next_sequence_;
	commands_.push_back(command);
}

} // namespace ptgn::impl