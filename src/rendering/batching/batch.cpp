#include "rendering/batching/batch.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "math/math.h"
#include "scene/camera.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"

namespace ptgn::impl {

Batch::Batch(const Shader& batch_shader, const Camera& batch_camera, BlendMode batch_blend_mode) :
	shader{ batch_shader }, camera{ batch_camera }, blend_mode{ batch_blend_mode } {}

void Batch::AddTexturedQuad(
	const std::array<V2_float, quad_vertex_count>& positions,
	const std::array<V2_float, quad_vertex_count>& tex_coords, float texture_index,
	const V4_float& color, const Depth& depth, bool pixel_rounding
) {
	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		Vertex v{};

		if (pixel_rounding) {
			v.position = { Round(positions[i].x), Round(positions[i].y),
						   static_cast<float>(depth) };
		} else {
			v.position = { positions[i].x, positions[i].y, static_cast<float>(depth) };
		}

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

void Batch::AddFilledEllipse(
	const std::array<V2_float, quad_vertex_count>& positions, const V4_float& color,
	const Depth& depth, bool pixel_rounding
) {
	AddTexturedQuad(positions, GetDefaultTextureCoordinates(), 1.0f, color, depth, pixel_rounding);
}

void Batch::AddHollowEllipse(
	const std::array<V2_float, quad_vertex_count>& positions, float line_width,
	const V2_float& radius, const V4_float& color, const Depth& depth, bool pixel_rounding
) {
	PTGN_ASSERT(line_width > 0.0f, "Cannot draw ellipse with negative line width");
	// Internally line width for a filled ellipse is 1.0f and a completely hollow one is 0.0f, but
	// in the API the line width is expected in pixels.
	// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected bugs.
	line_width = 0.005f + line_width / std::min(radius.x, radius.y);

	AddTexturedQuad(
		positions, GetDefaultTextureCoordinates(), line_width, color, depth, pixel_rounding
	);
}

void Batch::AddFilledTriangle(
	const std::array<V2_float, triangle_vertex_count>& positions, const V4_float& color,
	const Depth& depth, bool pixel_rounding
) {
	constexpr std::array<V2_float, triangle_vertex_count> tex_coords{
		V2_float{ 0.0f, 0.0f }, // lower-left corner
		V2_float{ 1.0f, 0.0f }, // lower-right corner
		V2_float{ 0.5f, 1.0f }, // top-center corner
	};

	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		Vertex v{};

		if (pixel_rounding) {
			v.position = { Round(positions[i].x), Round(positions[i].y),
						   static_cast<float>(depth) };
		} else {
			v.position = { positions[i].x, positions[i].y, static_cast<float>(depth) };
		}

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

void Batch::AddFilledQuad(
	const std::array<V2_float, quad_vertex_count>& positions, const V4_float& color,
	const Depth& depth, bool pixel_rounding
) {
	AddTexturedQuad(positions, GetDefaultTextureCoordinates(), 0.0f, color, depth, pixel_rounding);
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

bool Batch::Uses(const Shader& other_shader, const Camera& other_camera, BlendMode other_blend_mode)
	const {
	return shader == other_shader && blend_mode == other_blend_mode &&
		   camera == other_camera;
}

bool Batch::HasRoomForTexture(
	const Texture& texture, const Texture& white_texture, std::size_t max_texture_slots
) {
	return !texture.IsValid() ||
		   GetTextureIndex(texture.GetId(), white_texture.GetId(), max_texture_slots) != -1.0f;
}

bool Batch::HasRoomForShape(std::size_t vertex_count, std::size_t index_count) const {
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