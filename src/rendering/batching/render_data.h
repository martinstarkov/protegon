#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "core/entity.h"
#include "core/manager.h"
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
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
	float texture_index = 0.0f
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

class ShaderPass {
public:
	ShaderPass(
		const Shader& shader, const std::function<void(const Shader&)>& uniform_callback = nullptr
	);

	void Bind() const;

	[[nodiscard]] const Shader& GetShader() const;

	void Invoke() const;

	bool operator==(const ShaderPass& other) const;

	bool operator!=(const ShaderPass& other) const;

private:
	const Shader* shader_{ nullptr };
	std::function<void(const Shader&)> uniform_callback_;
};

class RenderState {
public:
	RenderState() = default;

	RenderState(
		const std::vector<ShaderPass>& shader_passes, BlendMode blend_mode, const Camera& camera
	) :
		shader_passes{ shader_passes }, blend_mode{ blend_mode }, camera{ camera } {}

	friend bool operator==(const RenderState& a, const RenderState& b) {
		return a.shader_passes == b.shader_passes && a.camera == b.camera &&
			   a.blend_mode == b.blend_mode;
	}

	friend bool operator!=(const RenderState& a, const RenderState& b) {
		return !(a == b);
	}

	std::vector<ShaderPass> shader_passes;
	BlendMode blend_mode{ BlendMode::None };
	Camera camera;
};

class RenderData {
public:
	void AddTriangle(const std::array<Vertex, 3>& vertices, const RenderState& state);

	void AddTexturedQuad(
		std::array<Vertex, 4>& vertices, const RenderState& state, TextureId texture_id
	);

	void AddQuad(const std::array<Vertex, 4>& vertices, const RenderState& state);

	void AddShader(const RenderState& render_state, BlendMode fbo_blendmode, bool ping_pong);

	RenderTarget screen_fbo;
	RenderTarget scene_fbo;
	RenderTarget effect_fbo;
	RenderTarget current_fbo;

private:
	friend class ptgn::Scene;
	friend class Renderer;
	friend class Camera;

	void SetCameraVertices(const Camera& camera);

	void Init();

	float GetTextureIndex(std::uint32_t texture_id);

	void SetState(const RenderState& new_render_state);

	void Flush();

	void Draw(Scene& scene);

	constexpr static std::array<Index, 6> camera_indices{ 0, 1, 2, 2, 3, 0 };
	constexpr static float min_line_width{ 1.0f };
	std::array<Vertex, 4> camera_vertices;
	Manager render_manager;
	RenderState render_state;
	std::vector<Vertex> vertices;
	std::vector<Index> indices;
	std::vector<TextureId> textures;
	Index index_offset{ 0 };
	std::size_t max_texture_slots{ 0 };
	Texture white_texture;
	VertexArray triangle_vao;
};

} // namespace impl

} // namespace ptgn