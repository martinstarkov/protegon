#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_renderer.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"
#include "utility/triangulation.h"

namespace ptgn::impl {

template class BatchData<QuadVertices, 6>;
template class BatchData<CircleVertices, 4>;
template class BatchData<TriangleVertices, 3>;
template class BatchData<LineVertices, 2>;
template class BatchData<PointVertices, 1>;

static constexpr std::size_t batch_capacity{ 2000 };

Batch::Batch(std::size_t max_texture_slots) : quad_{ max_texture_slots } {}

template <typename TVertices, std::size_t IndexCount>
bool BatchData<TVertices, IndexCount>::IsAvailable() const {
	return data_.size() != batch_capacity;
}

template <typename TVertices, std::size_t IndexCount>
TVertices& BatchData<TVertices, IndexCount>::Get() {
	PTGN_ASSERT(data_.size() + 1 <= batch_capacity);
	return data_.emplace_back();
}

template <typename TVertices, std::size_t IndexCount>
void BatchData<TVertices, IndexCount>::Flush(const RendererData& renderer) {
	if (!IsFlushed()) {
		PrepareBuffer(renderer);
		array_.GetVertexBuffer().SetSubData(
			data_.data(), static_cast<std::uint32_t>(data_.size()) * sizeof(TVertices)
		);
		GLRenderer::DrawElements(array_, data_.size() * IndexCount);
		data_.clear();
	}
}

template <typename TVertices, std::size_t IndexCount>
void BatchData<TVertices, IndexCount>::Clear() {
	data_.clear();
}

template <typename TVertices, std::size_t IndexCount>
void BatchData<TVertices, IndexCount>::SetupBuffer(
	PrimitiveMode type, const InternalBufferLayout& layout, std::size_t vertex_count,
	const IndexBuffer& index_buffer
) {
	if (!array_.IsValid()) {
		data_.reserve(batch_capacity);
		array_ = {
			type,
			VertexBuffer(
				data_.data(),
				static_cast<std::uint32_t>(batch_capacity * layout.GetStride() * vertex_count),
				BufferUsage::DynamicDraw
			),
			layout, index_buffer
		};
	}
}

template <typename TVertices, std::size_t IndexCount>
void BatchData<TVertices, IndexCount>::PrepareBuffer(const RendererData& renderer) {
	if constexpr (std::is_same_v<TVertices, QuadVertices>) {
		SetupBuffer(
			PrimitiveMode::Triangles, renderer.quad_layout, QuadVertices::count, renderer.quad_ib_
		);
	} else if constexpr (std::is_same_v<TVertices, CircleVertices>) {
		SetupBuffer(
			PrimitiveMode::Triangles, renderer.circle_layout, CircleVertices::count,
			renderer.quad_ib_
		);
	} else if constexpr (std::is_same_v<TVertices, TriangleVertices>) {
		SetupBuffer(
			PrimitiveMode::Triangles, renderer.color_layout, TriangleVertices::count,
			renderer.triangle_ib_
		);
	} else if constexpr (std::is_same_v<TVertices, LineVertices>) {
		SetupBuffer(
			PrimitiveMode::Lines, renderer.color_layout, LineVertices::count, renderer.line_ib_
		);
	} else if constexpr (std::is_same_v<TVertices, PointVertices>) {
		SetupBuffer(
			PrimitiveMode::Points, renderer.color_layout, PointVertices::count, renderer.point_ib_
		);
	} else {
		PTGN_ERROR("Failed to recognize batch buffer type");
	}
}

template <typename TVertices, std::size_t IndexCount>
bool BatchData<TVertices, IndexCount>::IsFlushed() const {
	return data_.empty();
}

TextureBatchData::TextureBatchData(std::size_t max_texture_slots) {
	// First texture slot is reserved for the empty white texture.
	textures_.reserve(max_texture_slots - 1);
}

void TextureBatchData::BindTextures() {
	for (std::uint32_t i{ 0 }; i < textures_.size(); i++) {
		// Save first texture slot for empty white texture.
		auto slot{ i + 1 };
		textures_[i].Bind(slot);
	}
}

std::pair<std::size_t, bool> TextureBatchData::GetTextureIndex(const Texture& t) {
	// Texture exists in batch, therefore do not readd it.
	for (std::size_t i = 0; i < textures_.size(); i++) {
		if (textures_[i] == t) {
			// First texture index is white texture.
			return { i + 1, true };
		}
	}
	// Texture does not exist in batch but can be added.
	if (HasAvailableTextureSlot()) {
		auto index{ textures_.size() + 1 };
		textures_.emplace_back(t);
		return { index, true };
	}
	// Texture does not exist in batch and batch is full.
	return { 0, false };
}

std::size_t TextureBatchData::GetTextureSlotCapacity() const {
	return textures_.capacity();
}

void TextureBatchData::Clear() {
	BatchData::Clear();
	textures_.clear();
}

bool TextureBatchData::HasAvailableTextureSlot() const {
	return textures_.size() != textures_.capacity();
}

bool Batch::IsFlushed(BatchType type) const {
	switch (type) {
		case BatchType::Quad:	  return quad_.IsFlushed();
		case BatchType::Triangle: return triangle_.IsFlushed();
		case BatchType::Line:	  return line_.IsFlushed();
		case BatchType::Circle:	  return circle_.IsFlushed();
		case BatchType::Point:	  return point_.IsFlushed();
		default:				  PTGN_ERROR("Failed to recognize batch type when checking IsFlushed");
	}
}

void Batch::Flush(const RendererData& renderer, BatchType type) {
	switch (type) {
		case BatchType::Quad:
			quad_.BindTextures();
			quad_.Flush(renderer);
			break;
		case BatchType::Triangle: triangle_.Flush(renderer); break;
		case BatchType::Line:	  line_.Flush(renderer); break;
		case BatchType::Circle:	  circle_.Flush(renderer); break;
		case BatchType::Point:	  point_.Flush(renderer); break;
		default:				  PTGN_ERROR("Failed to recognize batch type when flushing");
	}
}

bool Batch::IsAvailable(BatchType type) const {
	switch (type) {
		case BatchType::Quad:	  return quad_.IsAvailable();
		case BatchType::Triangle: return triangle_.IsAvailable();
		case BatchType::Line:	  return line_.IsAvailable();
		case BatchType::Circle:	  return circle_.IsAvailable();
		case BatchType::Point:	  return point_.IsAvailable();
		default:				  PTGN_ERROR("Failed to identify batch type when checking availability");
	}
}

void Batch::Clear() {
	quad_.Clear();
	circle_.Clear();
	triangle_.Clear();
	line_.Clear();
	point_.Clear();
}

void RendererData::Init() {
	auto get_indices = [](std::size_t max_indices, const auto& generator) {
		std::vector<std::uint32_t> indices;
		indices.resize(max_indices);
		std::generate(indices.begin(), indices.end(), generator);
		return indices;
	};

	constexpr std::array<std::uint32_t, 6> quad_index_pattern{ 0, 1, 2, 2, 3, 0 };

	auto quad_generator = [&quad_index_pattern, offset = 0, pattern_index = 0]() mutable {
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

	quad_ib_	 = { get_indices(batch_capacity * 6, quad_generator) };
	triangle_ib_ = { get_indices(batch_capacity * 3, iota) };
	line_ib_	 = { get_indices(batch_capacity * 2, iota) };
	point_ib_	 = { get_indices(batch_capacity * 1, iota) };

	// First texture slot is occupied by white texture
	white_texture_ = ptgn::Texture({ color::White }, { 1, 1 });

	max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	SetupShaders();
}

void RendererData::SetupShaders() {
	PTGN_ASSERT(max_texture_slots_ > 0, "Max texture slots must be set before setting up shaders");

	PTGN_INFO("Renderer Texture Slots: ", max_texture_slots_);
	// This strange way of including files allows for them to be packed into the library binary.
	ShaderSource quad_frag;

	if (max_texture_slots_ == 8) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_8.frag)
		};
	} else if (max_texture_slots_ == 16) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_16.frag)
		};
	} else if (max_texture_slots_ == 32) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_32.frag)
		};
	} else {
		PTGN_ERROR("Unsupported Texture Slot Size: ", max_texture_slots_);
	}

	quad_shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(quad.vert)
		},
		quad_frag
	);

	circle_shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(circle.vert)
		},
		ShaderSource{
#include PTGN_SHADER_PATH(circle.frag)
		}
	);

	color_shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
		},
		ShaderSource{
#include PTGN_SHADER_PATH(color.frag)
		}
	);

	std::vector<std::int32_t> samplers(max_texture_slots_);
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader_.Bind();
	quad_shader_.SetUniform("u_Textures", samplers.data(), samplers.size());
}

void RendererData::FlushLayer(RenderLayer& layer) {
	// TODO: Reduce shader rebinding by drawing all batches after binding.
	if (layer.new_view_projection) {
		quad_shader_.Bind();
		quad_shader_.SetUniform("u_ViewProjection", layer.view_projection);
		circle_shader_.Bind();
		circle_shader_.SetUniform("u_ViewProjection", layer.view_projection);
		color_shader_.Bind();
		color_shader_.SetUniform("u_ViewProjection", layer.view_projection);

		layer.new_view_projection = false;
	}

	// TODO: Add opaque batches back once you figure out how to do it using depth testing.
	// GLRenderer::EnableDepthTesting();
	// GLRenderer::EnableDepthWriting();
	// FlushBatches(opaque_batches_);
	// opaque_batches_.clear();
	// GLRenderer::DisableDepthWriting();

	PTGN_ASSERT(
		!layer.new_view_projection, "Opaque batch should have handled view projection reset"
	);
	// Flush batches in order of z_index.
	for (auto& [z_index, batches] : layer.batch_map) {
		FlushBatches(batches);
	}

	// TODO: Look into caching part of the batch, keeping around VAOs.
	// Confirm that this works as intended.
	// for (auto it = layer.batch_map.begin(); it != layer.batch_map.end();) {
	//	if (it->first == 0) {	  // z_index == 0
	//		it->second.resize(1); // shrink batch to only first batch.
	//		it->second.back().Clear();
	//		++it;
	//	} else {
	//		it = layer.batch_map.erase(it);
	//	}
	//}

	layer.batch_map.clear();
}

void RendererData::FlushBatches(std::vector<Batch>& batches) {
	Shader bound_shader;

	auto flush_batch_group = [&](const Shader& shader, BatchType type) {
		bool requires_flush{ false };

		for (auto& batch : batches) {
			if (!batch.IsFlushed(type)) {
				requires_flush = true;
			}
		}

		if (!requires_flush) {
			return;
		}

		PTGN_ASSERT(shader.IsValid(), "Cannot bind invalid shader");

		if (shader != bound_shader) {
			shader.Bind();
			bound_shader = shader;
			/*if (new_view_projection_) {
				shader.SetUniform("u_ViewProjection", view_projection_);
			}*/
		}

		PTGN_ASSERT(Shader::GetBoundId() != 0);

		for (auto& batch : batches) {
			batch.Flush(*this, type);
		}
	};

	std::invoke([&]() { flush_batch_group(quad_shader_, BatchType::Quad); });
	std::invoke([&]() { flush_batch_group(circle_shader_, BatchType::Circle); });
	std::invoke([&]() { flush_batch_group(color_shader_, BatchType::Triangle); });
	std::invoke([&]() { flush_batch_group(color_shader_, BatchType::Line); });
	std::invoke([&]() { flush_batch_group(color_shader_, BatchType::Point); });
}

void RendererData::Flush() {
	white_texture_.Bind(0);
	for (auto& [key, layer] : render_layers_) {
		FlushLayer(layer);
	}
}

std::array<V2_float, 4> RendererData::GetTextureCoordinates(
	const V2_float& source_position, V2_float source_size, const V2_float& texture_size, Flip flip
) {
	PTGN_ASSERT(texture_size.x > 0.0f, "Texture must have width > 0");
	PTGN_ASSERT(texture_size.y > 0.0f, "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	if (source_size.IsZero()) {
		source_size = texture_size - source_position;
	}

	// Convert to 0 -> 1 range.
	V2_float src_pos{ source_position / texture_size };
	V2_float src_size{ source_size / texture_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	V2_float half_pixel{ 0.5f / texture_size };

	std::array<V2_float, 4> texture_coordinates{
		src_pos + half_pixel,
		V2_float{ src_pos.x + src_size.x - half_pixel.x, src_pos.y + half_pixel.y },
		src_pos + src_size - half_pixel,
		V2_float{ src_pos.x + half_pixel.x, src_pos.y + src_size.y - half_pixel.y },
	};

	FlipTextureCoordinates(texture_coordinates, flip);

	return texture_coordinates;
}

Shader& RendererData::GetShader(BatchType type) {
	switch (type) {
		case BatchType::Quad:	  return quad_shader_;
		case BatchType::Triangle:
		case BatchType::Line:
		case BatchType::Point:	  return color_shader_;
		case BatchType::Circle:	  return circle_shader_;
		default:				  PTGN_ERROR("Failed to recognize shader batch type");
	}
}

RenderLayer& RendererData::GetRenderLayer(std::size_t render_layer) {
	if (auto it = render_layers_.find(render_layer); it != render_layers_.end()) {
		return it->second;
	}
	auto [new_it, _] = render_layers_.emplace(render_layer, RenderLayer{});
	return new_it->second;
}

void RendererData::AddQuad(
	const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const ptgn::Texture& t, std::size_t render_layer
) {
	if (t == white_texture_) {
		auto& batch_group = GetBatchGroup(GetRenderLayer(render_layer).batch_map, color.w, z_index);
		GetBatch(BatchType::Quad, batch_group).quad_.Get() =
			QuadVertices(vertices, z_index, color, tex_coords, 0.0f);
		return;
	}

	// Textures are always considered as part of the transparent batch groups.
	// In the future one could do a t.HasTransparency() check here to determine batch group.
	auto& batch_group = GetBatchGroup(GetRenderLayer(render_layer).batch_map, 0.0f, z_index);
	auto [batch, texture_index] = GetTextureBatch(batch_group, t);
	batch.quad_.Get() =
		QuadVertices(vertices, z_index, color, tex_coords, static_cast<float>(texture_index));
}

void RendererData::AddCircle(
	const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color, float line_width,
	float fade, std::size_t render_layer
) {
	auto& batch_group = GetBatchGroup(GetRenderLayer(render_layer).batch_map, color.w, z_index);
	GetBatch(BatchType::Circle, batch_group).circle_.Get() =
		CircleVertices(vertices, z_index, color, line_width, fade);
}

void RendererData::AddTriangle(
	const V2_float& a, const V2_float& b, const V2_float& c, float z_index, const V4_float& color,
	std::size_t render_layer
) {
	auto& batch_group = GetBatchGroup(GetRenderLayer(render_layer).batch_map, color.w, z_index);
	GetBatch(BatchType::Triangle, batch_group).triangle_.Get() =
		TriangleVertices({ a, b, c }, z_index, color);
}

void RendererData::AddLine(
	const V2_float& p0, const V2_float& p1, float z_index, const V4_float& color,
	std::size_t render_layer
) {
	auto& batch_group = GetBatchGroup(GetRenderLayer(render_layer).batch_map, color.w, z_index);
	GetBatch(BatchType::Line, batch_group).line_.Get() = LineVertices({ p0, p1 }, z_index, color);
}

void RendererData::AddPoint(
	const V2_float& position, float z_index, const V4_float& color, std::size_t render_layer
) {
	auto& batch_group = GetBatchGroup(GetRenderLayer(render_layer).batch_map, color.w, z_index);
	GetBatch(BatchType::Point, batch_group).point_.Get() =
		PointVertices({ position }, z_index, color);
}

std::vector<Batch>& RendererData::GetBatchGroup(BatchMap& batch_map, float alpha, float z_index) {
	// TODO: Add opaque batches back once you figure out how to do it using depth testing.
	/*
	if (NearlyEqual(alpha, 1.0f)) { // opaque object
		if (opaque_batches_.size() == 0) {
			opaque_batches_.emplace_back(max_texture_slots_);
		}
		return opaque_batches_;
	}
	*/
	// transparent object
	auto z_index_key{ static_cast<std::int64_t>(z_index) };
	if (auto it = batch_map.find(z_index_key); it != batch_map.end()) {
		return it->second;
	}
	std::vector<Batch> new_batch_group;
	new_batch_group.emplace_back(max_texture_slots_);
	auto new_it = batch_map.emplace(z_index_key, std::move(new_batch_group)).first;
	PTGN_ASSERT(!new_it->second.empty());
	PTGN_ASSERT(new_it->second.at(0).quad_.GetTextureSlotCapacity() == max_texture_slots_ - 1);
	return new_it->second;
}

Batch& RendererData::GetBatch(BatchType type, std::vector<Batch>& batch_group) {
	PTGN_ASSERT(!batch_group.empty());
	if (auto& latest_batch = batch_group.back(); latest_batch.IsAvailable(type)) {
		return latest_batch;
	}
	return batch_group.emplace_back(max_texture_slots_);
}

std::pair<Batch&, std::size_t> RendererData::GetTextureBatch(

	std::vector<Batch>& batch_group, const ptgn::Texture& t
) {
	PTGN_ASSERT(batch_group.size() > 0);
	PTGN_ASSERT(t.IsValid());
	for (std::size_t i{ 0 }; i < batch_group.size(); i++) {
		auto& batch = batch_group[i];
		if (batch.quad_.IsAvailable()) {
			auto [texture_index, has_available_index] = batch.quad_.GetTextureIndex(t);
			if (has_available_index) {
				return { batch, texture_index };
			}
		}
	}
	auto& new_batch{ batch_group.emplace_back(max_texture_slots_) };
	auto [texture_index, has_available_index] = new_batch.quad_.GetTextureIndex(t);
	PTGN_ASSERT(has_available_index);
	PTGN_ASSERT(texture_index == 1);
	return { new_batch, texture_index };
}

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip) {
	const auto flip_x = [&]() {
		std::swap(texture_coords[0].x, texture_coords[1].x);
		std::swap(texture_coords[2].x, texture_coords[3].x);
	};
	const auto flip_y = [&]() {
		std::swap(texture_coords[0].y, texture_coords[3].y);
		std::swap(texture_coords[1].y, texture_coords[2].y);
	};
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Horizontal: flip_x(); break;
		case Flip::Vertical:   flip_y(); break;
		case Flip::Both:
			flip_x();
			flip_y();
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
}

void RendererData::Texture(
	const std::array<V2_float, 4>& vertices, const ptgn::Texture& t,
	const std::array<V2_float, 4>& tex_coords, const V4_float& tint_color, float z,
	std::size_t render_layer
) {
	PTGN_ASSERT(t.IsValid(), "Cannot draw uninitialized or destroyed texture");
	AddQuad(vertices, z, tint_color, tex_coords, t, render_layer);
}

void RendererData::Ellipse(
	const V2_float& p, const V2_float& r, const V4_float& col, float lw, float z, float fade,
	std::size_t render_layer
) {
	PTGN_ASSERT(lw >= 0.0f || lw == -1.0f, "Cannot draw negative line width");

	auto vertices =
		GetQuadVertices(p, { r.x * 2.0f, r.y * 2.0f }, Origin::Center, 0.0f, { 0.5f, 0.5f });

	// Internally line width for a filled ellipse is 1.0f and a completely hollow one is 0.0f, but
	// in the API the line width is expected in pixels.
	// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected bugs.
	lw = NearlyEqual(lw, -1.0f) ? 1.0f : fade + lw / std::min(r.x, r.y);

	AddCircle(vertices, z, col, lw, fade, render_layer);
}

void RendererData::Line(
	const V2_float& p0, const V2_float& p1, const V4_float& col, float lw, float z,
	std::size_t render_layer
) {
	PTGN_ASSERT(lw >= 0.0f, "Cannot draw negative line width");

	if (lw > 1.0f) {
		V2_float d{ p1 - p0 };
		// TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
		auto vertices = GetQuadVertices(
			p0 + d * 0.5f, { d.Magnitude(), lw }, Origin::Center, d.Angle(), { 0.5f, 0.5f }
		);
		RendererData::Rectangle(vertices, col, -1.0f, z, render_layer);
		return;
	}

	AddLine(p0, p1, z, col, render_layer);
}

void RendererData::Point(
	const V2_float& p, const V4_float& col, float r, float z, std::size_t render_layer
) {
	if (r < 1.0f || NearlyEqual(r, 1.0f)) {
		AddPoint(p, z, col, render_layer);
	} else {
		RendererData::Ellipse(p, { r, r }, col, -1.0f, z, 0.005f, render_layer);
	}
}

void RendererData::Triangle(
	const V2_float& a, const V2_float& b, const V2_float& c, const V4_float& col, float lw, float z,
	std::size_t render_layer
) {
	if (lw == -1.0f) {
		AddTriangle(a, b, c, z, col, render_layer);
	} else {
		PTGN_ASSERT(lw >= 0.0f, "Cannot draw negative thickness triangle");
		std::array<V2_float, 3> vertices{ a, b, c };
		RendererData::Polygon(vertices.data(), vertices.size(), col, lw, z, render_layer);
	}
}

void RendererData::Rectangle(
	const std::array<V2_float, 4>& vertices, const V4_float& col, float lw, float z,
	std::size_t render_layer
) {
	if (lw == -1.0f) {
		RendererData::Texture(
			vertices, white_texture_,
			{
				V2_float{ 0.0f, 0.0f },
				V2_float{ 1.0f, 0.0f },
				V2_float{ 1.0f, 1.0f },
				V2_float{ 0.0f, 1.0f },
			},
			col, z, render_layer
		);
	} else {
		for (std::size_t i{ 0 }; i < vertices.size(); i++) {
			RendererData::Line(
				vertices[i], vertices[(i + 1) % vertices.size()], col, lw, z, render_layer
			);
		}
	}
}

void RendererData::RoundedRectangle(
	const V2_float& p, const V2_float& s, float rad, const V4_float& col, Origin o, float lw,
	float rotation_radians, const V2_float& rc, float z, std::size_t render_layer
) {
	PTGN_ASSERT(
		2.0f * rad < s.x, "Cannot draw rounded rectangle with larger radius than half its width"
	);
	PTGN_ASSERT(
		2.0f * rad < s.y, "Cannot draw rounded rectangle with larger radius than half its height"
	);

	V2_float offset = GetOffsetFromCenter(s, o);

	float rot{ rotation_radians };

	auto inner_vertices =
		GetQuadVertices(p - offset, s - V2_float{ rad * 2 }, Origin::Center, rot, rc);

	bool filled{ lw == -1.0f };

	float length{ rad };

	if (filled) {
		length = rad / 2.0f;
	};

	V2_float t = V2_float(length, 0.0f).Rotated(rot - half_pi<float>);
	V2_float r = V2_float(length, 0.0f).Rotated(rot + 0.0f);
	V2_float b = V2_float(length, 0.0f).Rotated(rot + half_pi<float>);
	V2_float l = V2_float(length, 0.0f).Rotated(rot - pi<float>);

	RendererData::Arc(
		inner_vertices[0], rad, rot - pi<float>, rot - half_pi<float>, false, col, lw, z,
		render_layer
	);
	RendererData::Arc(
		inner_vertices[1], rad, rot - half_pi<float>, rot + 0.0f, false, col, lw, z, render_layer
	);
	RendererData::Arc(
		inner_vertices[2], rad, rot + 0.0f, rot + half_pi<float>, false, col, lw, z, render_layer
	);
	RendererData::Arc(
		inner_vertices[3], rad, rot + half_pi<float>, rot + pi<float>, false, col, lw, z,
		render_layer
	);

	float line_thickness{ lw };

	if (filled) {
		RendererData::Rectangle(inner_vertices, col, lw, z, render_layer);
		line_thickness = rad;
	}

	RendererData::Line(
		inner_vertices[0] + t, inner_vertices[1] + t, col, line_thickness, z, render_layer
	);
	RendererData::Line(
		inner_vertices[1] + r, inner_vertices[2] + r, col, line_thickness, z, render_layer
	);
	RendererData::Line(
		inner_vertices[2] + b, inner_vertices[3] + b, col, line_thickness, z, render_layer
	);
	RendererData::Line(
		inner_vertices[3] + l, inner_vertices[0] + l, col, line_thickness, z, render_layer
	);
}

void RendererData::Arc(
	const V2_float& p, float arc_radius, float start_angle_radians, float end_angle_radians,
	bool clockwise, const V4_float& col, float lw, float z, std::size_t render_layer
) {
	PTGN_ASSERT(arc_radius >= 0.0f, "Cannot draw filled arc with negative radius");

	float start_angle = ClampAngle2Pi(start_angle_radians);
	float end_angle	  = ClampAngle2Pi(end_angle_radians);

	// Edge case where arc is a point.
	if (NearlyEqual(arc_radius, 0.0f)) {
		RendererData::Point(p, col, 1.0f, z, render_layer);
		return;
	}

	bool filled{ lw == -1.0f };

	PTGN_ASSERT(filled || lw > 0.0f, "Cannot draw arc with zero line thickness");

	// Edge case where start and end angles match (considered a full rotation).
	if (float range{ start_angle - end_angle };
		NearlyEqual(range, 0.0f) || NearlyEqual(range, two_pi<float>)) {
		RendererData::Ellipse(p, { arc_radius, arc_radius }, col, lw, z, 0.005f, render_layer);
	}

	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	float arc = end_angle - start_angle;

	// Number of triangles the arc is divided into.
	std::size_t resolution =
		std::max(static_cast<std::size_t>(360), static_cast<std::size_t>(30.0f * arc_radius));

	std::size_t n{ resolution };

	float delta_angle{ arc / static_cast<float>(n) };

	PTGN_ASSERT(arc >= 0.0f);

	if (n > 1) {
		std::vector<V2_float> v(n);

		for (std::size_t i{ 0 }; i < n; i++) {
			float angle_radians{ start_angle };
			float delta{ static_cast<float>(i) * delta_angle };
			if (clockwise) {
				angle_radians -= delta;
			} else {
				angle_radians += delta;
			}

			v[i] = { p.x + arc_radius * std::cos(angle_radians),
					 p.y + arc_radius * std::sin(angle_radians) };
		}

		if (filled) {
			for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
				RendererData::Triangle(p, v[i], v[i + 1], col, lw, z, render_layer);
			}
		} else {
			PTGN_ASSERT(lw >= 0.0f, "Must provide valid line width when drawing hollow arc");
			for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
				RendererData::Line(v[i], v[i + 1], col, lw, z, render_layer);
			}
		}
	} else {
		RendererData::Point(p, col, 1.0f, z, render_layer);
	}
}

void RendererData::Capsule(
	const V2_float& p0, const V2_float& p1, float r, const V4_float& col, float lw, float z,
	float fade, std::size_t render_layer
) {
	V2_float dir{ p1 - p0 };
	const float angle_radians{ dir.Angle() + half_pi<float> };
	const float dir2{ dir.Dot(dir) };

	V2_float tangent_r;

	// Note that dir2 is an int.
	if (NearlyEqual(dir2, 0.0f)) {
		RendererData::Ellipse(p0, { r, r }, col, lw, z, fade, render_layer);
		return;
	} else {
		V2_float tmp = dir.Skewed() / std::sqrt(dir2) * r;
		tangent_r	 = { FastFloor(tmp.x), FastFloor(tmp.y) };
	}

	float start_angle{ angle_radians };
	float end_angle{ angle_radians };

	if (lw == -1.0f) {
		// Draw central line.
		RendererData::Line(p0, p1, col, r * 2.0f, z, render_layer);

		// How many radians into the line the arc protrudes.
		constexpr float delta{ DegToRad(0.5f) };
		start_angle -= delta;
		end_angle	+= delta;
	} else {
		// Draw edge lines.
		RendererData::Line(p0 + tangent_r, p1 + tangent_r, col, lw, z, render_layer);
		RendererData::Line(p0 - tangent_r, p1 - tangent_r, col, lw, z, render_layer);
	}

	// Draw edge arcs.
	RendererData::Arc(p0, r, start_angle, end_angle + pi<float>, false, col, lw, z, render_layer);
	RendererData::Arc(p1, r, start_angle + pi<float>, end_angle, false, col, lw, z, render_layer);
}

void RendererData::Polygon(
	const V2_float* vertices, std::size_t vertex_count, const V4_float& col, float lw, float z,
	std::size_t render_layer
) {
	PTGN_ASSERT(vertices != nullptr);
	if (lw == -1.0f) {
		auto triangles = Triangulate(vertices, vertex_count);

		for (const auto& t : triangles) {
			RendererData::Triangle(t.a, t.b, t.c, col, lw, z, render_layer);
		}
	} else {
		for (std::size_t i{ 0 }; i < vertex_count; i++) {
			RendererData::Line(
				vertices[i], vertices[(i + 1) % vertex_count], col, lw, z, render_layer
			);
		}
	}
}

} // namespace ptgn::impl