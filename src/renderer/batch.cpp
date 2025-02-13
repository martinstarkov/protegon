#include "renderer/batch.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "components/draw.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/blend_mode.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "vfx/light.h"

namespace ptgn::impl {

Batch::Batch(const Shader& shader, const BlendMode& blend_mode) :
	shader{ shader }, blend_mode{ blend_mode } {}

void Batch::AddTexturedQuad(
	const std::array<V2_float, quad_vertex_count>& positions,
	const std::array<V2_float, quad_vertex_count>& tex_coords, float texture_index,
	const V4_float& color, const Depth& depth
) {
	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		Vertex v{};
		v.position	= { positions[i].x, positions[i].y, static_cast<float>(depth) };
		v.color		= { color.x, color.y, color.z, color.w };
		v.tex_coord = { tex_coords[i].x, tex_coords[i].y };
		v.tex_index = { texture_index };
		vertices.push_back(v);
	}
	indices.push_back(index_offset + 0);
	indices.push_back(index_offset + 1);
	indices.push_back(index_offset + 2);
	indices.push_back(index_offset + 2);
	indices.push_back(index_offset + 3);
	indices.push_back(index_offset + 0);
	index_offset += quad_vertex_count;
	PTGN_ASSERT(vertices.size() <= vertex_batch_capacity);
	PTGN_ASSERT(indices.size() <= index_batch_capacity);
}

void Batch::AddEllipse(
	const std::array<V2_float, quad_vertex_count>& positions,
	const std::array<V2_float, quad_vertex_count>& tex_coords, float line_width,
	const V2_float& radius, const V4_float& color, const Depth& depth
) {
	if (line_width == -1.0f) {
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width > 0.0f, "Cannot draw ellipse with negative line width");
		// Internally line width for a filled ellipse is 1.0f and a completely hollow one is 0.0f,
		// but in the API the line width is expected in pixels.
		// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected
		// bugs.
		line_width = 0.005f + line_width / std::min(radius.x, radius.y);
	}
	AddTexturedQuad(positions, tex_coords, line_width, color, depth);
}

void Batch::AddTriangle(
	const std::array<V2_float, triangle_vertex_count>& positions, const V4_float& color,
	const Depth& depth
) {
	constexpr std::array<V2_float, triangle_vertex_count> tex_coords{
		V2_float{ 0.0f, 0.0f }, // lower-left corner
		V2_float{ 1.0f, 0.0f }, // lower-right corner
		V2_float{ 0.5f, 1.0f }, // top-center corner
	};

	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		Vertex v{};
		v.position	= { positions[i].x, positions[i].y, static_cast<float>(depth) };
		v.color		= { color.x, color.y, color.z, color.w };
		v.tex_coord = { tex_coords[i].x, tex_coords[i].y };
		v.tex_index = { 0.0f };
		vertices.push_back(v);
	}
	indices.push_back(index_offset + 0);
	indices.push_back(index_offset + 1);
	indices.push_back(index_offset + 2);
	index_offset += triangle_vertex_count;
	PTGN_ASSERT(vertices.size() <= vertex_batch_capacity);
	PTGN_ASSERT(indices.size() <= index_batch_capacity);
}

float Batch::GetTextureIndex(
	std::uint32_t texture_id, std::uint32_t white_texture_id, std::size_t max_texture_slots
) {
	if (texture_id == white_texture_id) {
		return 0.0f;
	}
	// TextureManager::Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < texture_ids.size(); i++) {
		if (texture_ids[i] == texture_id) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	if (static_cast<std::uint32_t>(texture_ids.size()) == max_texture_slots - 1) {
		// TextureManager::Texture does not exist in batch and batch is full.
		return -1.0f;
	}
	// TextureManager::Texture does not exist in batch but can be added.
	texture_ids.emplace_back(texture_id);
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(texture_ids.size());
}

bool Batch::Uses(const Shader& other_shader, BlendMode other_blend_mode) const {
	return shader == other_shader && blend_mode == other_blend_mode;
}

bool Batch::HasRoomForTexture(
	const Texture& texture, const Texture& white_texture, std::size_t max_texture_slots
) {
	return !texture.IsValid() ||
		   GetTextureIndex(texture.GetId(), white_texture.GetId(), max_texture_slots) != -1.0f;
}

bool Batch::HasRoomForShape(ecs::Entity e) const {
	std::size_t vertex_count{ 0 };
	std::size_t index_count{ 0 };

	if (e.HasAny<TextureKey, Text, RenderTarget, Rect, Line, Circle, Ellipse, Point>()) {
		// Lines are rotated quads.
		// Points are either circles or quads.
		vertex_count = quad_vertex_count;
		index_count	 = quad_index_count;
	} else if (e.Has<Polygon>()) {
		const auto& polygon{ e.Get<Polygon>() };
		if (!e.Has<LineWidth>() || (e.Has<LineWidth>() && e.Get<LineWidth>() == -1.0f)) {
			// TODO: Figure out a better way to determine how many solid triangles this polygon
			// will have. I have not looked into the triangulation formula. It may just work
			// like a triangle fan.
			auto triangles{ polygon.Triangulate() };
			vertex_count = triangles.size() * triangle_vertex_count;
			index_count	 = triangles.size() * triangle_index_count;
		} else {
			// Hollow polygon.
			// Every line is a rotated quad.
			vertex_count = polygon.vertices.size() * quad_vertex_count;
			index_count	 = polygon.vertices.size() * quad_index_count;
		}
	} else if (e.Has<Triangle>()) {
		vertex_count = triangle_vertex_count;
		index_count	 = triangle_index_count;

	} else if (e.Has<Arc>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Arc drawing not implemented yet");
	} else if (e.Has<RoundedRect>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Rounded rectangle drawing not implemented yet");
	} else if (e.Has<Capsule>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Capsule drawing not implemented yet");
	}

	// Lights will not add vertices or indices.
	if (!e.Has<PointLight>()) {
		PTGN_ASSERT(vertex_count != 0);
		PTGN_ASSERT(index_count != 0);
	}

	return vertices.size() + vertex_count <= vertex_batch_capacity &&
		   indices.size() + index_count <= index_batch_capacity;
}

void Batch::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(texture_ids.size()); i++) {
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		Texture::Bind(texture_ids[i], slot);
	}
}

} // namespace ptgn::impl