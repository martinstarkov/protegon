#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/blend_mode.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_types.h"
#include "renderer/shader.h"
#include "renderer/texture.h"

namespace ptgn::impl {

struct Vertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	// For textures this is from 1 to max_texture_slots.
	// For solid triangles/quads this is 0 (white 1x1 texture).
	// For circles this stores the thickness: 0 is hollow, 1 is solid.
	glsl::float_ tex_index;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>
	quad_vertex_layout;

struct Batch {
	using IndexType = std::uint32_t;

	// Batch capacity based on quads because they are the most common shape.
	constexpr static inline std::uint32_t quad_batch_capacity{ 2000 };

	constexpr static inline std::uint32_t quad_vertex_count{ 4 };
	constexpr static inline std::uint32_t quad_index_count{ 6 };
	constexpr static inline std::uint32_t triangle_vertex_count{ 3 };
	constexpr static inline std::uint32_t triangle_index_count{ triangle_vertex_count };

	constexpr static inline std::uint32_t vertex_batch_capacity{ quad_batch_capacity *
																 quad_vertex_count };
	constexpr static inline std::uint32_t index_batch_capacity{ quad_index_count *
																vertex_batch_capacity };

	Batch(const Shader& shader, const BlendMode& blend_mode);

	const Shader& shader;
	BlendMode blend_mode;
	std::vector<std::uint32_t> texture_ids;
	std::vector<Vertex> vertices;
	std::vector<IndexType> indices;
	std::vector<ecs::Entity> lights;

	void AddTexturedQuad(
		const std::array<V2_float, quad_vertex_count>& positions,
		const std::array<V2_float, quad_vertex_count>& tex_coords, float texture_index,
		const V4_float& color, const Depth& depth
	);

	void AddFilledQuad(
		const std::array<V2_float, quad_vertex_count>& positions, const V4_float& color,
		const Depth& depth
	);

	void AddFilledEllipse(
		const std::array<V2_float, quad_vertex_count>& positions, const V4_float& color,
		const Depth& depth
	);

	void AddHollowEllipse(
		const std::array<V2_float, quad_vertex_count>& positions, float line_width,
		const V2_float& radius, const V4_float& color, const Depth& depth
	);

	void AddFilledTriangle(
		const std::array<V2_float, triangle_vertex_count>& positions, const V4_float& color,
		const Depth& depth
	);

	IndexType index_offset{ 0 };

	// return -1 if no available texture index
	float GetTextureIndex(
		std::uint32_t texture_id, std::uint32_t white_texture_id, std::size_t max_texture_slots
	);

	// @return True if the batch uses the specified shader and blend mode.
	bool Uses(const Shader& other_shader, BlendMode other_blend_mode) const;

	// @return True if the batch has room for the texture (or the texture id already exists in the
	// batch).
	bool HasRoomForTexture(
		const Texture& texture, const Texture& white_texture, std::size_t max_texture_slots
	);

	// @return True if batch has room for the number of vertices and indices.
	bool HasRoomForShape(std::size_t vertex_count, std::size_t index_count) const;

	void BindTextures() const;
};

struct Batches {
	std::vector<Batch> vector;
};

} // namespace ptgn::impl