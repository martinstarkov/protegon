#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <type_traits>
#include <vector>

#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "renderer/vertices.h"
#include "ui/plot.h"
#include "utility/debug.h"
#include "utility/log.h"
#include "utility/utility.h"

namespace ptgn {

// TODO: Figure out what to do with this.
struct Point {};

namespace impl {

// TODO: Replace with include.
struct PointLight {};

// TODO: Fix text.
struct Text {};

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

	Shader shader;
	BlendMode blend_mode;
	std::vector<std::uint32_t> texture_ids;
	std::vector<Vertex> vertices;
	std::vector<IndexType> indices;

	void AddTexturedQuad(
		const std::array<V2_float, quad_vertex_count>& positions,
		const std::array<V2_float, quad_vertex_count>& tex_coords, float texture_index,
		const V4_float& color, const Depth& depth
	);

	void AddQuad(
		const std::array<V2_float, quad_vertex_count>& positions, const V4_float& color,
		const Depth& depth
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
	void AddToBatch(
		Batch& batch, ecs::Entity e, Transform transform, const Depth& depth,
		const TextureManager::Texture& texture
	);

	void AddTexture(
		ecs::Entity e, const Transform& transform, const Depth& depth, const BlendMode& blend_mode,
		const TextureManager::Texture& texture, const Shader& shader
	);

	void DrawLight(ecs::Entity e);

	void FlushBatches();

	void PopulateBatches(ecs::Manager& manager);

	void Init();

	void Render(ecs::Manager& manager);

	std::size_t max_texture_slots{ 0 };

	TextureManager::Texture white_texture;

	// TODO: Fix.
	// RenderTarget lights;

	BlendMode default_blend_mode{ BlendMode::Blend };
	BlendMode light_blend_mode{ BlendMode::Add };

	// VertexArray window_vao;
	std::unique_ptr<VertexArray> triangle_vao;

	std::map<Depth, Batches> batch_map;
};

} // namespace impl

} // namespace ptgn