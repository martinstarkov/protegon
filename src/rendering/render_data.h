#pragma once

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "debug/profiling.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/api/vertex.h"
#include "rendering/buffers/buffer_layout.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/buffers/vertex_array.h"
#include "rendering/gl/gl_types.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"

#define HDR_ENABLED 0

namespace ptgn {

class Camera;
class Shader;
class Scene;
struct Matrix4;

// How the renderer resolution is scaled to the window size.
enum class ResolutionMode {
	Disabled,  /**< There is no scaling in effect */
	Stretch,   /**< The rendered content is stretched to the output resolution */
	Letterbox, /**< The rendered content is fit to the largest dimension and the other dimension is
				  letterboxed with black bars */
	Overscan,  /**< The rendered content is fit to the smallest dimension and the other dimension
				  extends beyond the output bounds */
	IntegerScale, /**< The rendered content is scaled up by integer multiples to fit the output
					 resolution */
};

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

constexpr std::size_t batch_capacity{ 10000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

[[nodiscard]] std::array<Vertex, 3> GetTriangleVertices(
	const std::array<V2_float, 3>& triangle_points, const Color& color, const Depth& depth
);

[[nodiscard]] std::array<Vertex, 4> GetQuadVertices(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
	float texture_index, std::array<V2_float, 4> texture_coordinates, bool flip_vertices = false
);

void SortEntities(std::vector<Entity>& entities);

void GetRenderArea(
	const V2_float& screen_size, const V2_float& target_size, ResolutionMode mode,
	V2_float& out_position, V2_float& out_size
);

using UniformCallback = void (*)(Entity, const Shader&);

class ShaderPass {
public:
	ShaderPass() = default;

	ShaderPass(const Shader& shader, UniformCallback uniform_callback = nullptr);

	[[nodiscard]] const Shader& GetShader() const;

	void Invoke(Entity entity) const;

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
		const ShaderPass& shader_pass, BlendMode blend_mode, const Camera& camera,
		const PostFX& post_fx = {}
	) :
		shader_pass{ shader_pass },
		blend_mode{ blend_mode },
		camera{ camera },
		post_fx{ post_fx } {}

	friend bool operator==(const RenderState& a, const RenderState& b) {
		return a.shader_pass == b.shader_pass && a.camera == b.camera &&
			   a.blend_mode == b.blend_mode && a.post_fx == b.post_fx;
	}

	friend bool operator!=(const RenderState& a, const RenderState& b) {
		return !(a == b);
	}

	ShaderPass shader_pass;
	BlendMode blend_mode{ BlendMode::None };
	Camera camera;
	PostFX post_fx;
};

struct DrawContext {
	DrawContext(const V2_int& size);

	FrameBuffer frame_buffer;

	bool in_use{ true };
	bool keep_alive{ false };

	BlendMode blend_mode{ BlendMode::Blend };

	Color clear_color{ color::Transparent };

	V2_int viewport_position;
	V2_int viewport_size;

	// Timer used to track age for reuse.
	Timer timer;
};

/*
 * 1. A spare DrawContext that has the same dimensions.
 * 2. A spare DrawContext that has not been used recently, resized.
 * 3. A new DrawContext, within the maximum pool size.
 * 4. The oldest spare DrawContext, resized.
 * 5. A new DrawContext, exceeding the maximum pool size.
 */
class DrawContextPool {
public:
	DrawContextPool(milliseconds max_age);

	// Retrieve a framebuffer of the given size.
	// Size must be positive and non-zero.
	std::shared_ptr<DrawContext> Get(V2_int size);

	// Clear and destroy all pooled framebuffers.
	void Clear();

	void TrimExpired();

	std::vector<std::shared_ptr<DrawContext>> contexts_;

private:
	milliseconds max_age_{ 0 };
};

class RenderData {
public:
	void AddPoint(
		const V2_float& position, const Color& tint, const Depth& depth, const RenderState& state
	);

	void AddLine(
		const V2_float& start, const V2_float& end, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddLines(
		const std::vector<V2_float>& line_points, const Color& tint, const Depth& depth,
		float line_width, bool connect_last_to_first, const RenderState& state
	);

	void AddTriangle(
		const std::array<V2_float, 3>& triangle_points, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddQuad(
		const Transform& transform, const V2_float& size, Origin origin, const Color& tint,
		const Depth& depth, float line_width, const RenderState& state
	);

	void AddPolygon(
		const std::vector<V2_float>& polygon_points, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddEllipse(
		const Transform& transform, const V2_float& radii, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddCircle(
		const Transform& transform, float radius, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddTexturedQuad(
		const Texture& texture, const Transform& transform, const V2_float& size, Origin origin,
		const Color& tint, const Depth& depth, const std::array<V2_float, 4>& texture_coordinates,
		const RenderState& state, const PreFX& pre_fx = {}
	);

	void AddShader(
		Entity entity, const RenderState& render_state, BlendMode target_blend_mode,
		const Color& target_clear_color, bool uses_scene_texture
	);

	void AddTemporaryTexture(Texture&& texture);

private:
	friend class ptgn::Scene;
	friend class Renderer;
	friend class ptgn::Camera;

	static void DrawTo(const FrameBuffer& frame_buffer);
	static void DrawTo(const RenderTarget& render_target);

	static void ReadFrom(const Texture& texture);
	static void ReadFrom(const FrameBuffer& frame_buffer);
	static void ReadFrom(const RenderTarget& render_target);

	static void BindCamera(const Shader& shader, const Matrix4& view_projection);
	static void SetViewport(const Camera& camera);

	static void SetRenderParameters(const Camera& camera, BlendMode blend_mode);

	template <typename T, typename S>
	void UpdateVertexArray(const T& point_vertices, const S& point_indices) {
		UpdateVertexArray(
			point_vertices.data(), point_vertices.size(), point_indices.data(), point_indices.size()
		);
	}

	template <typename T, typename S, typename U>
	void AddShape(
		const T& shape_vertices, const S& shape_indices, const U& shape_points, float line_width,
		const RenderState& state
	) {
		SetState(state);

		if (line_width == -1.0f) {
			AddVertices(shape_vertices, shape_indices);
		} else {
			AddLinesImpl(shape_vertices, shape_indices, shape_points, line_width, state);
		}
	}

	template <typename T, typename S, typename U>
	void AddLinesImpl(
		T line_vertices, const S& line_indices, const U& points, float line_width,
		[[maybe_unused]] const RenderState& state
	) {
		PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for lines");

		PTGN_ASSERT(points.size() == line_vertices.size());

		for (std::size_t i{ 0 }; i < points.size(); i++) {
			// TODO: Consider adding state.camera.ZoomIfNeeded(start&end);
			auto start{ points[i] };
			auto end{ points[(i + 1) % points.size()] };

			auto line_points{ impl::GetLineQuadVertices(start, end, line_width) };

			for (std::size_t j{ 0 }; j < line_points.size(); j++) {
				line_vertices[j].position[0] = line_points[j].x;
				line_vertices[j].position[1] = line_points[j].y;
			}

			AddVertices(line_vertices, line_indices);
		}
	}

	template <typename T, typename S>
	void AddVertices(const T& point_vertices, const S& point_indices) {
		if (vertices.size() + point_vertices.size() > vertex_capacity ||
			indices.size() + point_indices.size() > index_capacity) {
			Flush();
		}

		vertices.insert(vertices.end(), point_vertices.begin(), point_vertices.end());

		indices.reserve(indices.size() + point_indices.size());

		for (auto index : point_indices) {
			indices.emplace_back(index + index_offset);
		}

		index_offset += static_cast<Index>(point_vertices.size());
	}

	void InvokeDrawable(const Entity& entity);

	[[nodiscard]] Camera GetCamera(Scene& scene) const;

	void Reset();

	void BindTextures() const;

	void DrawVertexArray(std::size_t index_count) const;

	void UpdateVertexArray(
		const Vertex* data_vertices, std::size_t vertex_count, const Index* data_indices,
		std::size_t index_count
	);

	void SetCameraVertices(const Camera& camera);
	void SetCameraVertices(const std::array<V2_float, 4>& positions, const Depth& depth);

	void Init();

	// @param out_texture_index The available texture index for the texture id.
	// @return True if the texture_index currently exists in the textures vector, false if it must
	// be emplaced.
	[[nodiscard]] bool GetTextureIndex(std::uint32_t texture_id, float& out_texture_index);

	// @return True if the render state changed, false otherwise.
	bool SetState(const RenderState& new_render_state);

	// Uses current scene.
	void Flush();

	void Flush(Scene& scene);

	void DrawToScreen(Scene& scene);

	void DrawScene(Scene& scene);

	void Draw(Scene& scene);

	void ClearRenderTargets(Scene& scene) const;

	std::shared_ptr<DrawContext> intermediate_target;
	RenderTarget drawing_to;

	constexpr static float min_line_width{ 1.0f };
	constexpr static std::array<Index, 6> quad_indices{ 0, 1, 2, 2, 3, 0 };
	constexpr static std::array<Index, 3> triangle_indices{ 0, 1, 2 };
	// If true, will flush on the next state change regardless of state being new or not.
	bool force_flush{ false };

	// Default value results in fullscreen.
	V2_int resolution_;
	// Default value results in resolution_ being used.
	V2_int logical_resolution_;
	ResolutionMode scaling_mode_{ ResolutionMode::Disabled };

	std::array<Vertex, 4> camera_vertices;
	std::vector<Texture> temporary_textures;
	DrawContextPool draw_context_pool{ seconds{ 1 } };
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

PTGN_SERIALIZER_REGISTER_ENUM(
	ResolutionMode, { { ResolutionMode::Disabled, "disabled" },
					  { ResolutionMode::Stretch, "stretch" },
					  { ResolutionMode::Letterbox, "letterbox" },
					  { ResolutionMode::Overscan, "overscan" },
					  { ResolutionMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn