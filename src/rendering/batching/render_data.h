#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/batching/vertex.h"
#include "rendering/buffers/buffer_layout.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/buffers/vertex_array.h"
#include "rendering/gl/gl_types.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"

namespace ptgn {

class Shader;
class Scene;

struct QuadVertices {
	QuadVertices() = default;

	QuadVertices(const std::array<impl::Vertex, 4>& vertices) : vertices{ vertices } {}

	std::array<impl::Vertex, 4> vertices;
};

namespace impl {

class Renderer;

using Index = std::uint32_t;

constexpr std::array<V2_float, 4> default_texture_coordinates{
	V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 0.0f, 1.0f }
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>
	quad_vertex_layout;

constexpr std::size_t batch_capacity{ 4000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

[[nodiscard]] std::array<Vertex, 4> GetQuadVertices(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth
);

template <bool have_render_targets = false>
void SortEntities(std::vector<Entity>& entities) {
	std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
		auto depthA{ a.GetDepth() };
		auto depthB{ b.GetDepth() };

		if constexpr (!have_render_targets) {
			return depthA < depthB;
		}

		if (depthA != depthB) {
			return depthA < depthB; // Smaller depth first
		}

		PTGN_ASSERT(a.Has<RenderTarget>());
		PTGN_ASSERT(b.Has<RenderTarget>());

		// If depths are equal, compare framebuffer IDs
		auto idA{ a.Get<RenderTarget>().GetFrameBuffer().GetId() };
		auto idB{ b.Get<RenderTarget>().GetFrameBuffer().GetId() };
		return idA < idB;
	});
}

class Batch {
public:
	std::vector<Vertex> vertices;
	std::vector<Index> indices;
	std::vector<TextureId> textures;

	Index index_offset{ 0 };
};

class RenderState {
public:
	RenderState() = default;

	RenderState(
		const RenderTarget& render_target, const Shader* shader, BlendMode blend_mode,
		const Camera& camera
	) :
		render_target_{ render_target },
		shader_{ shader },
		blend_mode_{ blend_mode },
		camera_{ camera } {}

	friend bool operator==(const RenderState& a, const RenderState& b) {
		return a.shader_ == b.shader_ && a.camera_ == b.camera_ &&
			   a.render_target_ == b.render_target_ && a.blend_mode_ == b.blend_mode_;
	}

	friend bool operator!=(const RenderState& a, const RenderState& b) {
		return !(a == b);
	}

	RenderTarget render_target_;
	const Shader* shader_{ nullptr };
	BlendMode blend_mode_{ BlendMode::None };
	Camera camera_;
};

class RenderData {
public:
	void AddTriangle(const std::array<Vertex, 3>& vertices, const RenderState& state);

	void AddTexturedQuad(
		std::array<Vertex, 4>& vertices, const RenderState& state, TextureId texture_id
	);

	void AddQuad(const std::array<Vertex, 4>& vertices, const RenderState& state);

private:
	friend class ptgn::Scene;
	friend class Renderer;
	friend class Camera;

	void Init();

	float GetTextureIndex(std::uint32_t texture_id);

	void SetState(const RenderState& new_render_state);

	void Flush();

	void Draw(Scene& scene);

	constexpr static float min_line_width{ 1.0f };
	RenderState render_state;
	Batch batch;
	std::size_t max_texture_slots{ 0 };
	Texture white_texture;
	VertexArray triangle_vao;
};

} // namespace impl

} // namespace ptgn