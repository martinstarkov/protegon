#include "renderer/render_data.h"

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertices.h"
#include "utility/debug.h"
#include "utility/utility.h"

namespace ptgn::impl {

void RenderData::Init() {
	batch_capacity_ = 2000;

	auto get_indices = [](std::size_t max_indices, const auto& generator) {
		std::vector<std::uint32_t> indices;
		indices.resize(max_indices);
		std::generate(indices.begin(), indices.end(), generator);
		return indices;
	};

	constexpr std::array<std::uint32_t, 6> quad_index_pattern{ 0, 1, 2, 2, 3, 0 };

	auto quad_generator = [&quad_index_pattern, offset = static_cast<std::uint32_t>(0),
						   pattern_index = static_cast<std::size_t>(0)]() mutable {
		auto index = offset + quad_index_pattern[pattern_index];
		pattern_index++;
		if (pattern_index % quad_index_pattern.size() == 0) {
			offset		  += 4;
			pattern_index  = 0;
		}
		return index;
	};

	auto iota = [i = 0]() mutable {
		return i++;
	};

	quad_ib_	 = { get_indices(batch_capacity_ * 6, quad_generator) };
	triangle_ib_ = { get_indices(batch_capacity_ * 3, iota) };
	line_ib_	 = { get_indices(batch_capacity_ * 2, iota) };
	point_ib_	 = { get_indices(batch_capacity_ * 1, iota) };
	shader_ib_	 = { std::array<std::uint32_t, 6>{ 0, 1, 2, 2, 3, 0 } };

	// First texture slot is occupied by white texture
	white_texture_ = Texture({ color::White }, { 1, 1 });

	max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	quad_shader_   = game.shader.quad_;
	circle_shader_ = game.shader.circle_;
	color_shader_  = game.shader.color_;

	PTGN_ASSERT(quad_shader_.IsValid());
	PTGN_ASSERT(circle_shader_.IsValid());
	PTGN_ASSERT(color_shader_.IsValid());

	std::vector<std::int32_t> samplers(static_cast<std::size_t>(max_texture_slots_));
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader_.Bind();
	quad_shader_.SetUniform("u_Texture", samplers.data(), samplers.size());

	quad_vao_	  = VertexArray{ PrimitiveMode::Triangles,
							 VertexBuffer{ static_cast<std::array<QuadVertex, 4>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 quad_vertex_layout, quad_ib_ };
	circle_vao_	  = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<CircleVertex, 4>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 circle_vertex_layout, quad_ib_ };
	triangle_vao_ = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 3>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, triangle_ib_ };
	line_vao_	  = VertexArray{ PrimitiveMode::Lines,
							 VertexBuffer{ static_cast<std::array<ColorVertex, 2>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 color_vertex_layout, line_ib_ };
	point_vao_	  = VertexArray{ PrimitiveMode::Points,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 1>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, point_ib_ };
}

void RenderData::Reset() {
	quad_vao_	  = {};
	circle_vao_	  = {};
	triangle_vao_ = {};
	line_vao_	  = {};
	point_vao_	  = {};

	quad_shader_   = {};
	circle_shader_ = {};
	color_shader_  = {};

	quad_ib_	 = {};
	triangle_ib_ = {};
	line_ib_	 = {};
	point_ib_	 = {};
	shader_ib_	 = {};

	batch_capacity_	   = 0;
	max_texture_slots_ = 0;
	white_texture_	   = {};
}

// Assumes view_projection_ is updated externally.
void RenderData::Flush() {
	PTGN_ASSERT(quad_shader_.IsValid());
	PTGN_ASSERT(circle_shader_.IsValid());
	PTGN_ASSERT(color_shader_.IsValid());

	if (transparent_layers_.empty() /* TODO: && opaque_layers_.empty() */) {
		return;
	}

	white_texture_.Bind(0);

	// TODO: Figure out if there is a way to cache when a view projection has been updated. The
	// problem is that these shaders can be used elsewhere in the engine which means that the
	// u_ViewProjection could be set elsewhere. Perhaps the solution is to cache a refresh flag for
	// each uniform within a shader somehow.
	if (update_circle_shader_) {
		circle_shader_.Bind();
		circle_shader_.SetUniform("u_ViewProjection", view_projection_);
		update_circle_shader_ = false;
	}
	if (update_color_shader_) {
		color_shader_.Bind();
		color_shader_.SetUniform("u_ViewProjection", view_projection_);
		update_color_shader_ = false;
	}
	if (update_quad_shader_) {
		quad_shader_.Bind();
		quad_shader_.SetUniform("u_ViewProjection", view_projection_);
		update_quad_shader_ = true;
	}

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

const Matrix4& RenderData::GetViewProjection() const {
	return view_projection_;
}

void RenderData::AddPrimitiveQuad(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture
) {
	AddPrimitive<BatchType::Quad, QuadVertex>(
		positions, render_layer, color, tex_coords,
		texture.IsValid() ? texture : white_texture_, 0.0f, 0.0f
	);
	update_quad_shader_ = true;
}

void RenderData::AddPrimitiveCircle(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	float line_width, float fade
) {
	AddPrimitive<BatchType::Circle, CircleVertex>(
		positions, render_layer, color, {}, {}, line_width, fade
	);
	update_circle_shader_ = true;
}

void RenderData::AddPrimitiveTriangle(
	const std::array<V2_float, 3>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Triangle, ColorVertex>(positions, render_layer, color);
	update_color_shader_ = true;
}

void RenderData::AddPrimitiveLine(
	const std::array<V2_float, 2>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Line, ColorVertex>(positions, render_layer, color);
	update_color_shader_ = true;
}

void RenderData::AddPrimitivePoint(
	const std::array<V2_float, 1>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Point, ColorVertex>(positions, render_layer, color);
	update_color_shader_ = true;
}

bool RenderData::IsBlank(const Texture& texture) {
	return texture == white_texture_;
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

template <BatchType T>
Shader RenderData::GetShader() {
	// Triangles, lines, and points all share the same color shader.
	if constexpr (T == BatchType::Triangle || T == BatchType::Line || T == BatchType::Point) {
		return color_shader_;
	} else if constexpr (T == BatchType::Quad) {
		return quad_shader_;
	} else if constexpr (T == BatchType::Circle) {
		return circle_shader_;
	}
}

template <BatchType T>
void RenderData::FlushType(std::vector<Batch>& batches) const {
	auto vao{ GetVertexArray<T>() };
	for (auto& batch : batches) {
		auto [data, index_count] = GetBufferInfo<T>(batch);
		PTGN_ASSERT(data != nullptr);
		// Batch is empty for this specific type.
		if (data->empty()) {
			continue;
		}
		auto shader{ GetShader<T>() };
		// Renderer keeps track of bound shader and bound vertex array and ensures that they are not
		// rebound repeatedly for each batch.
		vao.Bind();
		shader.Bind();
		if constexpr (T == BatchType::Quad) {
			batch.BindTextures();
		}
		vao.GetVertexBuffer().SetSubData(
			data->data(), static_cast<std::uint32_t>(Sizeof(*data)), false
		);
		vao.Draw(data->size() * index_count, false);
		// data->clear(); // Not needed since transparent_layers_ is cleared every frame.
	}
}

template void RenderData::FlushType<BatchType::Circle>(std::vector<Batch>& batches) const;
template void RenderData::FlushType<BatchType::Quad>(std::vector<Batch>& batches) const;
template void RenderData::FlushType<BatchType::Triangle>(std::vector<Batch>& batches) const;
template void RenderData::FlushType<BatchType::Line>(std::vector<Batch>& batches) const;
template void RenderData::FlushType<BatchType::Point>(std::vector<Batch>& batches) const;
template Shader RenderData::GetShader<BatchType::Circle>();
template Shader RenderData::GetShader<BatchType::Quad>();
template Shader RenderData::GetShader<BatchType::Triangle>();
template Shader RenderData::GetShader<BatchType::Line>();
template Shader RenderData::GetShader<BatchType::Point>();

void RenderData::FlushBatches(std::vector<Batch>& batches) {
	PTGN_ASSERT(!batches.empty(), "Attempting to flush an empty batch");

	FlushType<BatchType::Circle>(batches);
	FlushType<BatchType::Triangle>(batches);
	FlushType<BatchType::Line>(batches);
	FlushType<BatchType::Point>(batches);
	FlushType<BatchType::Quad>(batches);
}

template <BatchType T, typename VertexType, std::size_t VertexCount>
void RenderData::AddPrimitive(
	const std::array<V2_float, VertexCount>& positions, std::int32_t render_layer,
	const V4_float& color, const std::array<V2_float, 4>& tex_coords, const Texture& texture,
	float line_width, float fade
) {
	PTGN_ASSERT(color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f && color.w >= 0.0f);
	PTGN_ASSERT(color.x <= 1.0f && color.y <= 1.0f && color.z <= 1.0f && color.w <= 1.0f);
	PTGN_ASSERT(fade >= 0.0f);

	// @return pair.first is the vector to which the primitive can be added, pair.second is the
	// texture index.
	auto get_available_batch = [&](float alpha) {
		float texture_index{ 0.0f };

		constexpr bool is_quad{ std::is_same_v<VertexType, QuadVertex> };

		// Textures are currently always considered part of the transparent batches.
		// TODO: Check texture format (e.g. RGB888) to separate it into the opaque batches.
		auto& batches{ GetLayerBatches(render_layer, is_quad ? 0.0f : alpha) };
		PTGN_ASSERT(!batches.empty());

		std::vector<std::array<VertexType, VertexCount>>* data{ nullptr };

		if constexpr (is_quad) {
			if (IsBlank(texture)) {
				data = GetBufferInfo<T>(batches.back()).first;
				if (data->size() == batch_capacity_) {
					data = GetBufferInfo<T>(batches.emplace_back()).first;
				}
			} else {
				PTGN_ASSERT(texture.IsValid());

				for (auto& batch : batches) {
					if (batch.quads_.size() == batch_capacity_) {
						continue;
					}
					if (auto index{ batch.GetAvailableTextureIndex(texture) }; index != 0.0f) {
						data		  = &batch.quads_;
						texture_index = index;
						break;
					}
				}
				// An available/existing texture index was not found, therefore add a new
				// batch.
				if (texture_index == 0.0f) {
					texture_index = 1.0f;
					data		  = &batches.emplace_back(texture).quads_;
				}
			}
		} else {
			data = GetBufferInfo<T>(batches.back()).first;
			if (data->size() == batch_capacity_) {
				data = GetBufferInfo<T>(batches.emplace_back()).first;
			}
		}

		PTGN_ASSERT(data != nullptr);
		PTGN_ASSERT(data->size() + 1 <= batch_capacity_);

		return std::pair<std::vector<std::array<VertexType, VertexCount>>&, float>{ *data,
																					texture_index };
	};

	auto [data, texture_index] = get_available_batch(color.w);

	// Used for circle vertices.
	constexpr std::array<V2_float, 4> local{ V2_float{ -1.0f, -1.0f }, V2_float{ 1.0f, -1.0f },
											 V2_float{ 1.0f, 1.0f }, V2_float{ -1.0f, 1.0f } };

	std::array<VertexType, VertexCount> vertices;

	for (std::size_t i{ 0 }; i < VertexCount; i++) {
		vertices[i].position = { positions[i].x, positions[i].y, static_cast<float>(render_layer) };
		vertices[i].color	 = { color.x, color.y, color.z, color.w };
		if constexpr (std::is_same_v<VertexType, QuadVertex>) {
			vertices[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
			vertices[i].tex_index = { texture_index };
		} else if constexpr (std::is_same_v<VertexType, CircleVertex>) {
			// local z coordinate provided for memory alignment.
			vertices[i].local_position = { local[i].x, local[i].y, 0.0f };
			vertices[i].line_width	   = { line_width };
			vertices[i].fade		   = { fade };
		}
	}

	data.emplace_back(vertices);
}

template void RenderData::AddPrimitive<BatchType::Quad, QuadVertex, 4>(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture, float line_width, float fade
);

template void RenderData::AddPrimitive<BatchType::Circle, CircleVertex, 4>(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture, float line_width, float fade
);

template void RenderData::AddPrimitive<BatchType::Triangle, ColorVertex, 3>(
	const std::array<V2_float, 3>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture, float line_width, float fade
);

template void RenderData::AddPrimitive<BatchType::Line, ColorVertex, 2>(
	const std::array<V2_float, 2>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture, float line_width, float fade
);

template void RenderData::AddPrimitive<BatchType::Point, ColorVertex, 1>(
	const std::array<V2_float, 1>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture, float line_width, float fade
);

} // namespace ptgn::impl