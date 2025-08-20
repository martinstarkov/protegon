#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/effects.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/geometry/line.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/api/vertex.h"
#include "renderer/buffers/buffer_layout.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/buffers/vertex_array.h"
#include "renderer/gl/gl_types.h"
#include "renderer/render_target.h"
#include "renderer/texture.h"
#include "scene/camera.h"

#define HDR_ENABLED 0

namespace ptgn {

class Camera;
class Shader;
class Scene;
struct Matrix4;

// How the renderer resolution is scaled to the window size.
enum class LogicalResolutionMode {
	Disabled,  /**< There is no scaling in effect */
	Stretch,   /**< The rendered content is stretched to the output resolution */
	Letterbox, /**< The rendered content is fit to the largest dimension and the other dimension is
				  letterboxed with black bars */
	Overscan,  /**< The rendered content is fit to the smallest dimension and the other dimension
				  extends beyond the output bounds */
	IntegerScale, /**< The rendered content is scaled up by integer multiples to fit the output
					 resolution */
};

struct Viewport {
	V2_int position;
	V2_int size;

	bool operator==(const Viewport&) const = default;
};

namespace impl {

class InputHandler;
class Renderer;
class SceneManager;

struct ViewportResizeScript : public Script<ViewportResizeScript, WindowScript> {
	void OnWindowResized() override;
};

using Index			= std::uint32_t;
using TextureOrSize = std::variant<std::reference_wrapper<const impl::Texture>, V2_int>;

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

using UniformCallback = void (*)(Entity, const Shader&);

class ShaderPass {
public:
	ShaderPass() = default;

	ShaderPass(const Shader& shader, UniformCallback uniform_callback = nullptr);

	[[nodiscard]] const Shader& GetShader() const;

	void Invoke(Entity entity) const;

	bool operator==(const ShaderPass&) const = default;

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
	);

	bool operator==(const RenderState&) const = default;

	ShaderPass shader_pass;
	BlendMode blend_mode{ BlendMode::None };
	Camera camera;
	PostFX post_fx;
};

struct ShapeDrawInfo {
	explicit ShapeDrawInfo(const Entity& entity);

	Transform transform;
	Color tint;
	Depth depth;
	LineWidth line_width;
	RenderState state;
};

struct DrawContext {
	DrawContext(const V2_int& size);

	FrameBuffer frame_buffer;

	bool in_use{ true };
	bool keep_alive{ false };

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
		const Texture& texture, Transform transform, const V2_float& size, Origin origin,
		const Color& tint, const Depth& depth, const std::array<V2_float, 4>& texture_coordinates,
		const RenderState& state, const PreFX& pre_fx = {}
	);

	// @param texture_or_size If texture, uses texture size, otherwise uses the V2_int size or the
	// scene render target size (physical resolution).
	// @param clear_between_consecutive_calls Will clear the intermediate render target between
	// consecutive calls. This prevents stacking of shader calls onto the same target. An example of
	// where this is not desired is when rendering many lights back to back. Does not apply if a
	// texture is used.
	void AddShader(
		Entity entity, const RenderState& render_state, const Color& target_clear_color,
		const TextureOrSize& texture_or_size = V2_int{}, const Color& tint = color::White,
		bool clear_between_consecutive_calls = true
	);

	void AddTemporaryTexture(Texture&& texture);

private:
	friend class SceneManager;
	friend class ptgn::Scene;
	friend class Renderer;
	friend class ptgn::Camera;
	friend struct ViewportResizeScript;
	friend class InputHandler;

	[[nodiscard]] static V2_float GetResolutionScale(const V2_float& viewport_size);

	[[nodiscard]] V2_float RelativeToViewport(const V2_float& window_relative_point) const;

	// TODO: Replace with std::span.

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

			Line l{ start, end };

			auto line_points{ l.GetWorldQuadVertices(Transform{}, line_width) };

			for (std::size_t j{ 0 }; j < line_vertices.size(); j++) {
				line_vertices[j].position[0] = line_points[j].x;
				line_vertices[j].position[1] = line_points[j].y;
			}

			AddVertices(line_vertices, line_indices);
		}
	}

	template <typename T, typename S>
	void AddVertices(const T& point_vertices, const S& point_indices) {
		if (vertices_.size() + point_vertices.size() > vertex_capacity ||
			indices_.size() + point_indices.size() > index_capacity) {
			Flush();
		}

		vertices_.insert(vertices_.end(), point_vertices.begin(), point_vertices.end());

		indices_.reserve(indices_.size() + point_indices.size());

		for (auto index : point_indices) {
			indices_.emplace_back(index + index_offset_);
		}

		index_offset_ += static_cast<Index>(point_vertices.size());
	}

	void InvokeDrawable(const Entity& entity);

	[[nodiscard]] TextureId PingPong(
		const std::vector<Entity>& container, const std::shared_ptr<DrawContext>& read_context,
		const std::array<V2_float, 4>& points, const Depth& depth, const Texture& texture,
		const V2_int& viewport_position, const V2_int& viewport_size,
		const Matrix4& view_projection, bool flip_vertices
	);

	void DrawCall(
		const Shader& shader, std::span<const Vertex> vertices, std::span<const Index> indices,
		const std::vector<TextureId>& textures, const impl::FrameBuffer& frame_buffer,
		bool clear_frame_buffer, const Color& clear_color, BlendMode blend_mode,
		const V2_int& viewport_position, const V2_int& viewport_size, const Matrix4& view_projection
	);

	void Reset();

	void Init();

	// @param out_texture_index The available texture index for the texture id.
	// @return True if the texture_index currently exists in the textures vector, false if it must
	// be emplaced.
	[[nodiscard]] bool GetTextureIndex(std::uint32_t texture_id, float& out_texture_index);

	// @return True if the render state changed, false otherwise.
	bool SetState(const RenderState& new_render_state);

	void RecomputeViewport(const V2_int& window_size);

	// Uses current scene.
	void Flush();

	void Flush(const Scene& scene);

	void DrawToScreen(Scene& scene);

	void DrawScene(Scene& scene);

	void Draw(Scene& scene);

	void ClearRenderTargets(Scene& scene) const;

	// TODO: Clean this up.

	std::shared_ptr<DrawContext> intermediate_target;
	RenderTarget drawing_to;

	static constexpr float min_line_width{ 1.0f };
	static constexpr std::array<Index, 6> quad_indices{ 0, 1, 2, 2, 3, 0 };
	static constexpr std::array<Index, 3> triangle_indices{ 0, 1, 2 };
	// If true, will flush on the next state change regardless of state being new or not.
	bool force_flush{ false };

	void UpdateResolutions(
		const V2_int& logical_resolution, LogicalResolutionMode logical_resolution_mode
	);

	bool logical_resolution_set_{ false };
	LogicalResolutionMode resolution_mode_{ LogicalResolutionMode::Letterbox };
	V2_int logical_resolution_;
	Viewport physical_viewport_;
	bool logical_resolution_changed_{ false };
	bool physical_resolution_changed_{ false };

	Entity viewport_tracker;

	std::vector<Texture> temporary_textures;
	DrawContextPool draw_context_pool{ seconds{ 1 } };
	Manager render_manager;
	RenderState render_state;
	std::vector<Vertex> vertices_;
	std::vector<Index> indices_;
	std::vector<TextureId> textures_;
	Index index_offset_{ 0 };
	std::size_t max_texture_slots{ 0 };
	Texture white_texture;
	VertexArray triangle_vao;
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	LogicalResolutionMode, { { LogicalResolutionMode::Disabled, "disabled" },
							 { LogicalResolutionMode::Stretch, "stretch" },
							 { LogicalResolutionMode::Letterbox, "letterbox" },
							 { LogicalResolutionMode::Overscan, "overscan" },
							 { LogicalResolutionMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn