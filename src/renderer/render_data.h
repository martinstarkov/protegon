#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/blend_mode.h"
#include "renderer/buffer_layout.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_types.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"

namespace ptgn {

// TODO: Figure out what to do with this.
struct Point {};

namespace impl {

// TODO: Replace with include.
struct PointLight {};

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

	void AddTexturedQuad(
		const std::array<V2_float, quad_vertex_count>& positions,
		const std::array<V2_float, quad_vertex_count>& tex_coords, float texture_index,
		const V4_float& color, const Depth& depth
	);

	void AddEllipse(
		const std::array<V2_float, quad_vertex_count>& positions,
		const std::array<V2_float, quad_vertex_count>& tex_coords, float line_width,
		const V2_float& radius, const V4_float& color, const Depth& depth
	);

	void AddTriangle(
		const std::array<V2_float, triangle_vertex_count>& positions, const V4_float& color,
		const Depth& depth
	);

	IndexType index_offset{ 0 };

	// return -1 if no available texture index
	float GetTextureIndex(
		std::uint32_t white_texture_id, std::size_t max_texture_slots, std::uint32_t texture_id
	);

	// True if batch has room for these.
	bool CanAccept(ecs::Entity e) const;

	void BindTextures() const;
};

struct Batches {
	std::vector<Batch> vector;
	ecs::Entity prev_light;
};

class RenderData {
public:
	void Init();

	void Render(const FrameBuffer& frame_buffer, const Camera& camera, ecs::Manager& manager);
	void Render(
		const FrameBuffer& frame_buffer, const Camera& camera, ecs::Entity e, bool check_visibility
	);

private:
	void AddToBatch(
		Batch& batch, ecs::Entity e, Transform transform, const Depth& depth, const Texture& texture
	);

	void AddTexture(
		ecs::Entity e, const Transform& transform, const Depth& depth, const BlendMode& blend_mode,
		const Texture& texture, const Shader& shader
	);

	void DrawLight(ecs::Entity e);

	void SetupRender(const FrameBuffer& frame_buffer, const Camera& camera) const;
	void PopulateBatches(ecs::Entity entity, bool check_visibility);
	void FlushBatches(const FrameBuffer& frame_buffer, const Camera& camera);

	std::size_t max_texture_slots{ 0 };

	Texture white_texture;

	// TODO: Fix.
	// RenderTarget lights;

	BlendMode default_blend_mode{ BlendMode::Blend };
	BlendMode light_blend_mode{ BlendMode::Add };

	// VertexArray window_vao;
	VertexArray triangle_vao;

	std::map<Depth, Batches> batch_map;
};

} // namespace impl

} // namespace ptgn