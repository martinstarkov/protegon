#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <variant>
#include <vector>

#include "components/draw.h"
#include "components/effects.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/resolution.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/api/vertex.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/buffers/vertex_array.h"
#include "renderer/render_target.h"
#include "renderer/texture.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class Camera;
class Shader;
class Scene;

struct Matrix4;
struct Capsule;
struct Arc;
struct Circle;
struct Ellipse;
struct RoundedRect;
struct Rect;
struct Line;
struct Polygon;
struct Triangle;

struct Viewport {
	Viewport() = default;
	Viewport(const V2_int& position, const V2_int& size);

	V2_int position;
	V2_int size;

	bool operator==(const Viewport&) const = default;

	PTGN_SERIALIZER_REGISTER(Viewport, position, size)
};

namespace impl {

class InputHandler;
class Renderer;
class SceneManager;

struct ViewportResizeScript : public Script<ViewportResizeScript, WindowScript> {
	void OnWindowResized() override;
};

using Index			= std::uint32_t;
using TextureOrSize = std::variant<std::reference_wrapper<const Texture>, V2_int>;

constexpr std::size_t batch_capacity{ 10000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

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
	DrawContext(const V2_int& size, TextureFormat texture_format);

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
	std::shared_ptr<DrawContext> Get(V2_int size, TextureFormat texture_format);

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
		const Transform& transform, const V2_float& position, const Color& tint, const Depth& depth,
		const RenderState& state
	);

	void AddLine(
		const Transform& transform, const Line& line, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddLines(
		const Transform& transform, const std::vector<V2_float>& line_points, const Color& tint,
		const Depth& depth, float line_width, bool connect_last_to_first, const RenderState& state
	);

	void AddCapsule(
		const Transform& transform, const Capsule& capsule, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddArc(
		const Transform& transform, const Arc& arc, bool clockwise, const Color& tint,
		const Depth& depth, float line_width, const RenderState& state
	);

	void AddTriangle(
		const Transform& transform, const Triangle& triangle, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddQuad(
		const Transform& transform, const Rect& rect, Origin origin, const Color& tint,
		const Depth& depth, float line_width, const RenderState& state
	);

	void AddRoundedQuad(
		const Transform& transform, const RoundedRect& rrect, Origin origin, const Color& tint,
		const Depth& depth, float line_width, const RenderState& state
	);

	void AddPolygon(
		const Transform& transform, const Polygon& polygon, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddEllipse(
		const Transform& transform, const Ellipse& ellipse, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddCircle(
		const Transform& transform, const Circle& circle, const Color& tint, const Depth& depth,
		float line_width, const RenderState& state
	);

	void AddTexturedQuad(
		const Transform& transform, const Texture& texture, const Rect& rect, Origin origin,
		const Color& tint, const Depth& depth, const std::array<V2_float, 4>& texture_coordinates,
		const RenderState& state, const PreFX& pre_fx = {}
	);

	// @param texture_or_size If texture, uses texture size, otherwise uses the V2_int size or the
	// scene render target size (display size).
	// @param clear_between_consecutive_calls Will clear the intermediate render target between
	// consecutive calls. This prevents stacking of shader calls onto the same target. An example of
	// where this is not desired is when rendering many lights back to back. Does not apply if a
	// texture is used.
	// @param blend_mode The blend mode with which each shader call is blended to the intermediate
	// target.
	// @param texture_format Specify a custom texture format to use for the shader.
	void AddShader(
		Entity entity, const RenderState& render_state, const Color& target_clear_color,
		const TextureOrSize& texture_or_size = V2_int{},
		bool clear_between_consecutive_calls = true, BlendMode blend_mode = BlendMode::Blend,
		TextureFormat texture_format = TextureFormat::RGBA8888
	);

	void AddTemporaryTexture(Texture&& texture);

	[[nodiscard]] std::size_t GetMaxTextureSlots() const;

private:
	friend class SceneManager;
	friend class ptgn::Scene;
	friend class Renderer;
	friend class ptgn::Camera;
	friend struct ViewportResizeScript;
	friend class InputHandler;

	struct DrawTarget {
		Viewport viewport;
		V2_int texture_size;
		TextureId texture_id{ 0 };
		TextureFormat texture_format{ TextureFormat::RGBA8888 };
		const FrameBuffer* frame_buffer{ nullptr };
		std::array<V2_float, 4> points{};
		Depth depth;
		Tint tint;
		Matrix4 view_projection{ 1.0f };
		BlendMode blend_mode{};
	};

	[[nodiscard]] static float GetFade(float diameter_y);

	static float NormalizeArcLineWidthToThickness(
		float line_width, float fade, const V2_float& radii
	);

	static float GetAspectRatio(const V2_float& size);
	static float GetNormalizedRadius(float diameter, float size_x);

	static void SetPointsAndProjection(DrawTarget& target);

	[[nodiscard]] static DrawTarget GetDrawTarget(const RenderTarget& render_target);

	[[nodiscard]] static DrawTarget GetDrawTarget(
		const RenderTarget& render_target, const Matrix4& view_projection,
		const std::array<V2_float, 4>& points, bool use_viewport
	);

	void AddShape(
		std::span<const Vertex> shape_vertices, std::span<const Index> shape_indices,
		std::span<const V2_float> shape_points, float line_width, const RenderState& state
	);

	void AddLinesImpl(
		std::span<Vertex> line_vertices, std::span<const Index> line_indices,
		std::span<const V2_float> points, float line_width, const RenderState& state
	);

	void AddVertices(std::span<const Vertex> point_vertices, std::span<const Index> point_indices);

	void InvokeDrawable(const Entity& entity);

	/*
	 * Applies a sequence of shader effects (e.g., post-processing passes) by ping-ponging between
	 * two framebuffers. The final result is written into a texture, whose ID is returned.
	 *
	 * This is typically used in screen-space effects like blur, bloom, color grading, etc.
	 *
	 * @param container       A list of entities, each containing a ShaderPass component that
	 * defines a rendering effect to apply.
	 * @param read_context    The initial draw context used for reading. This context contains the
	 *                        framebuffer with the source texture or result of previous passes.
	 * @param texture         The initial texture to apply effects to (e.g., scene render target).
	 *                        Must be valid for the first pass.
	 * @param target          A DrawTarget containing viewport info and configuration for rendering
	 *                        each shader pass.
	 * @param flip_vertices   If true, flips the output texture quad vertically (useful for
	 * screen-space coordinate correction).
	 *
	 * @return The texture ID containing the final rendered result after all shader passes.
	 *
	 * @note The function uses a ping-pong approach by alternating between two framebuffers:
	 *       one for reading, one for writing. This avoids unnecessary GPU memory allocations.
	 *
	 * @warning The input container must not be empty. An assertion will fail if it is.
	 */
	[[nodiscard]] TextureId PingPong(
		const std::vector<Entity>& container, const std::shared_ptr<DrawContext>& read_context,
		const Texture& texture, DrawTarget target, bool flip_vertices
	);

	/**
	 * Draws a set of previously prepared vertices using the specified shader and render target.
	 *
	 * @param shader              The shader to use for rendering.
	 * @param target              The rendering target that contains the destination framebuffer,
	 *                            blend mode, viewport, etc.
	 * @param clear_frame_buffer  If true, the target's framebuffer will be cleared before drawing.
	 */
	void DrawVertices(
		const Shader& shader, const RenderData::DrawTarget& target, bool clear_frame_buffer
	);

	/**
	 * Draws a fullscreen quad using the provided shader, commonly used for post-processing effects.
	 *
	 * @param shader              The shader to apply to the fullscreen quad.
	 * @param target              The rendering target that contains the destination framebuffer,
	 *                            blend mode, viewport, etc.
	 * @param flip_texture        If true, vertically flips the texture coordinates (useful when
	 * rendering to screen-space or handling texture origin discrepancies).
	 * @param clear_frame_buffer  If true, clears the framebuffer to the specified clear color
	 * before drawing.
	 * @param target_clear_color  The color used to clear the framebuffer, if clearing is enabled.
	 */
	void DrawFullscreenQuad(
		const Shader& shader, const RenderData::DrawTarget& target, bool flip_texture,
		bool clear_frame_buffer, const Color& target_clear_color
	);

	/**
	 * Issues a low-level draw call with the given vertex and index data, rendering to the specified
	 * framebuffer.
	 *
	 * @param shader              The shader to bind and use during the draw call.
	 * @param vertices            The list of vertices to draw.
	 * @param indices             The list of indices that define how the vertices are connected.
	 * @param textures            The textures to bind and make available to the shader.
	 * @param frame_buffer        The destination framebuffer to draw into. If null, draws to the
	 * default framebuffer.
	 * @param clear_frame_buffer  If true, clears the framebuffer to the specified clear color
	 * before drawing.
	 * @param clear_color         The color to clear the framebuffer with, if clearing is enabled.
	 * @param blend_mode          The blend mode to use during rendering (e.g., alpha blending,
	 * additive, etc.).
	 * @param viewport            The portion of the framebuffer to draw to.
	 * @param view_projection     The matrix used to transform vertex positions into screen space.
	 */
	void DrawCall(
		const Shader& shader, std::span<const Vertex> vertices, std::span<const Index> indices,
		const std::vector<TextureId>& textures, const FrameBuffer* frame_buffer,
		bool clear_frame_buffer, const Color& clear_color, BlendMode blend_mode,
		const Viewport& viewport, const Matrix4& view_projection
	);

	void Reset();

	void Init();

	// @param out_texture_index The available texture index for the texture id.
	// @return True if the texture_index currently exists in the textures vector, false if it must
	// be emplaced.
	[[nodiscard]] bool GetTextureIndex(std::uint32_t texture_id, float& out_texture_index);

	// @return True if the render state changed, false otherwise.
	bool SetState(const RenderState& new_render_state);

	void RecomputeDisplaySize(const V2_int& window_size);

	void Flush();

	void DrawScene(Scene& scene);

	void Draw(Scene& scene);

	/**
	 * Draws a textured quad using the contents of a source render target onto a destination
	 * framebuffer.
	 *
	 * This is typically used for compositing, where one render target  is drawn onto another buffer
	 * or the screen target.
	 *
	 * @param source_target       The render target whose texture will be drawn.
	 * @param points              The four corner points (in destination space) of the quad onto
	 * which the source target will be mapped. These should be in the order: top-left, top-right,
	 * bottom-right, bottom-left.
	 * @param projection          The projection matrix that transforms the quad's points into clip
	 * space. Defines the coordinate system used during drawing.
	 * @param viewport            The portion of the destination framebuffer that will be rendered
	 * to.
	 * @param destination_buffer  The framebuffer that the source target will be drawn into.
	 *                            If null, draws to the default framebuffer (usually the screen).
	 */
	void DrawFromTo(
		const RenderTarget& source_target, const std::array<V2_float, 4>& points,
		const Matrix4& projection, const Viewport& viewport, const FrameBuffer* destination_buffer,
		bool flip_texture
	);

	// Draws the screen target to the default frame buffer.
	void DrawScreenTarget();

	// @param filter If function returns true, the entity is not drawn.
	void DrawDisplayList(
		const RenderTarget& render_target, std::vector<Entity>& display_list,
		const std::function<bool(const Entity&)>& filter = {}
	);

	void ClearScreenTarget() const;

	// Clear the scene's internal render target, and all of the render target objects that exist in
	// the scene.
	void ClearRenderTargets(Scene& scene) const;

	const Shader& GetFullscreenShader(TextureFormat texture_format);

	std::shared_ptr<DrawContext> intermediate_target;

	DrawTarget drawing_to_;

	// TODO: Clean this up.

	static constexpr float min_line_width{ 1.0f };
	static constexpr std::array<Index, 6> quad_indices{ 0, 1, 2, 2, 3, 0 };
	static constexpr std::array<Index, 3> triangle_indices{ 0, 1, 2 };
	// If true, will flush on the next state change regardless of state being new or not.
	bool force_flush{ false };

	void UpdateResolutions(const V2_int& game_size, ScalingMode scaling_mode);

	bool game_size_set_{ false };
	ScalingMode resolution_mode_{ ScalingMode::Letterbox };

	// Allow for creation of targets before window has been initialized.
	V2_int game_size_{ 1, 1 };
	Viewport display_viewport_{ {}, { 1, 1 } };

	bool game_size_changed_{ false };
	bool display_size_changed_{ false };

	RenderTarget screen_target_;
	Entity viewport_tracker;

	std::vector<Texture> temporary_textures;
	DrawContextPool draw_context_pool{ seconds{ 1 } };
	Manager render_manager;
	RenderState render_state;
	std::vector<Vertex> vertices_;
	std::vector<Index> indices_;
	std::vector<TextureId> textures_;
	Index index_offset_{ 0 };
	// Cached variable.
	mutable std::size_t max_texture_slots{ 0 };
	Texture white_texture;
	VertexArray triangle_vao;
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	ScalingMode, { { ScalingMode::Disabled, "disabled" },
				   { ScalingMode::Stretch, "stretch" },
				   { ScalingMode::Letterbox, "letterbox" },
				   { ScalingMode::Overscan, "overscan" },
				   { ScalingMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn