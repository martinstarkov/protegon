#pragma once

#include <array>
#include <cstdint>
#include <iterator>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "core/entity.h"
#include "core/manager.h"
#include "math/vector2.h"
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
struct Matrix4;

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
	float texture_index, std::array<V2_float, 4> texture_coordinates, bool flip_vertices = false
);

template <bool have_render_targets = false>
void SortEntities(std::vector<Entity>& entities) {
	std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
		auto depthA{ a.GetDepth() };
		auto depthB{ b.GetDepth() };

		if constexpr (!have_render_targets) {
			return depthA < depthB;
		} else {
			if (depthA != depthB) {
				return depthA < depthB; // Smaller depth first
			}

			PTGN_ASSERT(a.Has<RenderTarget>());
			PTGN_ASSERT(b.Has<RenderTarget>());

			// If depths are equal, compare framebuffer IDs
			auto idA{ a.Get<RenderTarget>().GetFrameBuffer().GetId() };
			auto idB{ b.Get<RenderTarget>().GetFrameBuffer().GetId() };
			return idA < idB;
		}
	});
}

using UniformCallback = void (*)(const Entity&, const Shader&);

class ShaderPass {
public:
	ShaderPass(const Shader& shader, UniformCallback uniform_callback = nullptr);

	[[nodiscard]] const Shader& GetShader() const;

	void Invoke(const Entity& entity) const;

	bool operator==(const ShaderPass& other) const;

	bool operator!=(const ShaderPass& other) const;

private:
	const Shader* shader_{ nullptr };
	UniformCallback uniform_callback_{ nullptr };
};

class RenderState {
public:
	RenderState() = default;

	RenderState(
		const std::vector<ShaderPass>& shader_passes, BlendMode blend_mode, const Camera& camera,
		const std::vector<ShaderPass>& pre_fx = {}, const std::vector<ShaderPass>& post_fx = {}
	) :
		shader_passes{ shader_passes },
		blend_mode{ blend_mode },
		camera{ camera },
		pre_fx{ pre_fx },
		post_fx{ post_fx } {}

	friend bool operator==(const RenderState& a, const RenderState& b) {
		return a.shader_passes == b.shader_passes && a.camera == b.camera &&
			   a.blend_mode == b.blend_mode && a.pre_fx == b.pre_fx && a.post_fx == b.post_fx;
	}

	friend bool operator!=(const RenderState& a, const RenderState& b) {
		return !(a == b);
	}

	std::vector<ShaderPass> shader_passes;
	BlendMode blend_mode{ BlendMode::None };
	Camera camera;
	std::vector<ShaderPass> pre_fx;
	std::vector<ShaderPass> post_fx;
};

class RenderData {
public:
	template <typename T, typename S>
	void UpdateVertexArray(const T& point_vertices, const S& point_indices) {
		UpdateVertexArray(
			point_vertices.data(), point_vertices.size(), point_indices.data(), point_indices.size()
		);
	}

	template <typename T, typename S>
	void AddVertices(const T& point_vertices, const S& point_indices) {
		if (vertices.size() + point_vertices.size() > vertex_capacity ||
			indices.size() + point_indices.size() > index_capacity) {
			Flush();
		}

		vertices.insert(vertices.end(), point_vertices.begin(), point_vertices.end());

		std::transform(
			point_indices.begin(), point_indices.end(), std::back_inserter(indices),
			[=](auto x) { return x + index_offset; }
		);

		index_offset += static_cast<Index>(point_vertices.size());
	}

	void AddTriangle(const std::array<Vertex, 3>& vertices, const RenderState& state);

	void AddTexturedQuad(
		std::array<Vertex, 4>& vertices, const RenderState& state, TextureId texture_id
	);

	void AddQuad(const std::array<Vertex, 4>& vertices, const RenderState& state);

	void AddShader(
		const Entity& entity, const RenderState& render_state, BlendMode target_blend_mode,
		const Color& target_clear_color, bool uses_scene_texture
	);

	RenderTarget screen_fbo;
	RenderTarget scene_fbo;
	RenderTarget effect_fbo;
	RenderTarget current_fbo;

	constexpr static float min_line_width{ 1.0f };

private:
	friend class ptgn::Scene;
	friend class Renderer;
	friend class Camera;

	static void DrawTo(const FrameBuffer& frame_buffer);
	static void DrawTo(const RenderTarget& render_target);

	static void ReadFrom(const Texture& texture);
	static void ReadFrom(const FrameBuffer& frame_buffer);
	static void ReadFrom(const RenderTarget& render_target);

	static void BindCamera(const Shader& shader, const Matrix4& view_projection);
	static void SetViewport(const Camera& camera);

	static void SetRenderParameters(const Camera& camera, BlendMode blend_mode);

	void DrawShaders(const Entity& entity, const Camera& camera) const;

	void DrawToRenderTarget(
		const Entity& entity, const RenderTarget& rt, BlendMode blend_mode, const Color& clear_color
	);

	[[nodiscard]] Camera GetCamera(const Camera& fallback) const;

	[[nodiscard]] RenderTarget GetPingPongTarget() const;

	void DrawEntities(const std::vector<Entity>& entities);

	void FlushCurrentTarget();
	void FlushBatch();

	void Reset();

	void BindTextures() const;

	void DrawVertexArray(std::size_t index_count) const;

	void UpdateVertexArray(
		const Vertex* data_vertices, std::size_t vertex_count, const Index* data_indices,
		std::size_t index_count
	);

	void SetCameraVertices(const Camera& camera);

	void Init();

	float GetTextureIndex(std::uint32_t texture_id);

	void SetState(const RenderState& new_render_state);

	void Flush();

	void DrawToScreen();

	void DrawScene(Scene& scene);

	void Draw(Scene& scene);

	void ClearRenderTargets(Scene& scene);

	constexpr static std::array<Index, 6> quad_indices{ 0, 1, 2, 2, 3, 0 };
	constexpr static std::array<Index, 3> triangle_indices{ 0, 1, 2 };
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