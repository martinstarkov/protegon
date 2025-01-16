#include "renderer/render_data.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/game.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/buffer.h"
#include "renderer/gl_types.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "renderer/vertices.h"
#include "utility/debug.h"
#include "utility/triangulation.h"
#include "utility/utility.h"

namespace ptgn {

namespace impl {

Batch::Batch(const Texture& texture) {
	textures_.emplace_back(texture);
}

void Batch::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(textures_.size()); i++) {
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		textures_[i].Bind(slot);
	}
}

float Batch::GetAvailableTextureIndex(const Texture& texture) {
	PTGN_ASSERT(!RenderData::IsBlank(texture));
	PTGN_ASSERT(texture.IsValid());
	// Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < textures_.size(); i++) {
		if (textures_[i] == texture) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	if (static_cast<std::int32_t>(textures_.size()) == game.renderer.max_texture_slots_ - 1) {
		// Texture does not exist in batch and batch is full.
		return 0.0f;
	}
	// Texture does not exist in batch but can be added.
	textures_.emplace_back(texture);
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(textures_.size());
}

RenderData::RenderData() : batch_capacity_{ game.renderer.batch_capacity_ } {
	PTGN_ASSERT(batch_capacity_ >= 1);
	quad_vao_	  = VertexArray{ PrimitiveMode::Triangles,
							 VertexBuffer{ static_cast<std::array<QuadVertex, 4>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 quad_vertex_layout, game.renderer.quad_ib_ };
	circle_vao_	  = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<CircleVertex, 4>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 circle_vertex_layout, game.renderer.quad_ib_ };
	triangle_vao_ = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 3>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, game.renderer.triangle_ib_ };
	line_vao_	  = VertexArray{ PrimitiveMode::Lines,
							 VertexBuffer{ static_cast<std::array<ColorVertex, 2>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 color_vertex_layout, game.renderer.line_ib_ };
	point_vao_	  = VertexArray{ PrimitiveMode::Points,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 1>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, game.renderer.point_ib_ };
}

// Assumes view_projection_ is updated externally.
void RenderData::Flush() {
	PTGN_ASSERT(game.renderer.quad_shader_.IsValid());
	PTGN_ASSERT(game.renderer.circle_shader_.IsValid());
	PTGN_ASSERT(game.renderer.color_shader_.IsValid());

	game.renderer.white_texture_.Bind(0);

	if (transparent_layers_.empty()) {
		return;
	}

	// TODO: Figure out if there is a way to cache when a view projection has been updated.
	game.renderer.circle_shader_.Bind();
	game.renderer.circle_shader_.SetUniform("u_ViewProjection", view_projection_);
	game.renderer.color_shader_.Bind();
	game.renderer.color_shader_.SetUniform("u_ViewProjection", view_projection_);
	game.renderer.quad_shader_.Bind();
	game.renderer.quad_shader_.SetUniform("u_ViewProjection", view_projection_);

	// TODO: Add opaque batches back once you figure out how to do it using depth testing.
	// auto was_depth_testing{ GLRenderer::IsDepthTestingEnabled() };
	// auto was_depth_writing{ GLRenderer::IsDepthWritingEnabled() };
	// if (!was_depth_testing) {
	//	GLRenderer::EnableDepthTesting();
	// }
	// if (!was_depth_writing) {
	//	GLRenderer::EnableDepthWriting();
	// }
	// FlushBatches(opaque_batches_);
	// opaque_batches_.clear();
	// TODO: Check which of these is necessary to be disabled for the transparent batch below.
	// GLRenderer::DisableDepthWriting();
	// GLRenderer::DisableDepthTesting();
	// TODO: Make sure to uncomment re-enabling of depth writing after transparent batch is done
	// flushing (see below).
	// PTGN_ASSERT(!new_view_projection_, "Opaque batch should have handled view projection
	// reset");

	// Flush transparent layers in order of render layer.
	for (auto& [render_layer, batches] : transparent_layers_) {
		FlushBatches(batches);
	}

	// TODO: Re-enable when opaque batching is figured out
	// if (was_depth_testing) {
	//	GLRenderer::EnableDepthTesting();
	// }
	// if (was_depth_writing) {
	//	GLRenderer::EnableDepthWriting();
	// }

	transparent_layers_.clear();
}

bool RenderData::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ == view_projection) {
		return false;
	}
	view_projection_ = view_projection;
	return true;
}

void RenderData::AddPrimitiveQuad(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture
) {
	AddPrimitive<BatchType::Quad, QuadVertex>(
		positions, render_layer, color, tex_coords,
		texture.IsValid() ? texture : game.renderer.white_texture_, 0.0f, 0.0f
	);
}

void RenderData::AddPrimitiveCircle(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	float line_width, float fade
) {
	AddPrimitive<BatchType::Circle, CircleVertex>(
		positions, render_layer, color, {}, {}, line_width, fade
	);
}

void RenderData::AddPrimitiveTriangle(
	const std::array<V2_float, 3>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Triangle, ColorVertex>(positions, render_layer, color);
}

void RenderData::AddPrimitiveLine(
	const std::array<V2_float, 2>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Line, ColorVertex>(positions, render_layer, color);
}

void RenderData::AddPrimitivePoint(
	const std::array<V2_float, 1>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Point, ColorVertex>(positions, render_layer, color);
}

bool RenderData::IsBlank(const Texture& texture) {
	return texture == game.renderer.white_texture_;
}

std::vector<Batch>& RenderData::GetLayerBatches(
	std::int32_t render_layer, [[maybe_unused]] float alpha
) {
	// TODO: Add opaque batches back once you figure out how to do it using depth testing.
	/*
	// Transparent object.
	if (NearlyEqual(alpha, 1.0f)) { // opaque object
		if (opaque_batches_.size() == 0) {
			opaque_batches_.emplace_back(max_texture_slots_);
		}
		return opaque_batches_;
	}
	*/
	// Transparent object.
	if (auto it{ transparent_layers_.find(render_layer) }; it != transparent_layers_.end()) {
		return it->second;
	}
	return transparent_layers_.emplace(render_layer, std::vector<Batch>(1)).first->second;
}

void RenderData::FlushBatches(std::vector<Batch>& batches) {
	game.renderer.quad_shader_.Bind();

	FlushType<BatchType::Quad>(batches);

	game.renderer.circle_shader_.Bind();

	FlushType<BatchType::Circle>(batches);

	// Triangles, points, and lines all use color shader so only one bind is necessary.
	game.renderer.color_shader_.Bind();

	FlushType<BatchType::Triangle>(batches);
	FlushType<BatchType::Line>(batches);
	FlushType<BatchType::Point>(batches);
}

} // namespace impl

} // namespace ptgn