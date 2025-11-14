#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/app/manager.h"
#include "core/app/resolution.h"
#include "core/asset/asset_handle.h"
#include "core/ecs/components/effects.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/text/font.h"
#include "serialization/json/enum.h"
#include "serialization/json/serializable.h"
#include "world/scene/camera.h"

namespace ptgn {

class Renderer;
class Window;
class Shader;
class Scene;
class RenderTarget;
class Shape;
class Entity;
struct Capsule;
struct Arc;
struct Circle;
struct Ellipse;
struct RoundedRect;
struct Rect;
struct Line;
struct Polygon;
struct Triangle;

constexpr BlendMode default_blend_mode{ BlendMode::Blend };
constexpr Origin default_origin{ Origin::Center };

namespace impl {

namespace gl {

class GLContext;

} // namespace gl

struct ViewportResizeScript : public Script<ViewportResizeScript, WindowScript> {
	explicit ViewportResizeScript(Window& window, Renderer& renderer);

	void OnWindowResized() override;

	Window& window;
	Renderer& renderer;
};

using Index			= std::uint32_t;
using TextureOrSize = std::variant<std::reference_wrapper<const Handle<Texture>>, V2_int>;

constexpr std::size_t batch_capacity{ 10000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

using UniformCallback = void (*)(Entity, gl::GLContext&, const Handle<Shader>&);

struct ShaderPass {
	bool operator==(const ShaderPass&) const = default;

	Handle<Shader> shader;
	UniformCallback uniform_callback{ nullptr };
};

class RenderState {
public:
	RenderState() = default;

	RenderState(
		const ShaderPass& shader_pass, BlendMode blend_mode, const Camera& camera,
		const PostFX& post_fx = {}
	);

	// @return True if the render state is set, false if it has been reset (no shader pass
	// specified).
	[[nodiscard]] bool IsSet() const;

	bool operator==(const RenderState&) const = default;

	// std::nullopt = reset RenderState; ShaderPass{} == Quad shader.
	std::optional<ShaderPass> shader_pass{ ShaderPass{} };
	BlendMode blend_mode{ BlendMode::ReplaceRGBA };
	Camera camera;
	PostFX post_fx;
};

struct DrawContext {
	DrawContext(const V2_int& size, TextureFormat texture_format);

	FrameBuffer frame_buffer;

	std::optional<BlendMode> blend_mode{ std::nullopt };
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

struct DrawTarget {
	Viewport viewport;
	V2_int texture_size;
	TextureId texture_id{ 0 };
	TextureFormat texture_format{ TextureFormat::RGBA8888 };
	// TODO: Use something other than pointer here.
	const FrameBuffer* frame_buffer{ nullptr };
	std::array<V2_float, 4> points{};
	Depth depth;
	Tint tint;
	Matrix4 view_projection{ 1.0f };
	BlendMode blend_mode{};
};

struct DrawShapeCommand {
	Shape shape;
	Transform transform;
	Depth depth;
	Tint tint;
	LineWidth line_width;
	Origin origin{ default_origin };
	RenderState render_state;
};

struct DrawLinesCommand {
	std::vector<V2_float> points;
	bool connect_last_to_first{ false };
	Transform transform;
	Depth depth;
	Tint tint;
	LineWidth line_width;
	RenderState render_state;
};

struct DrawTextureCommand {
	TextureId texture_id{ 0 };
	V2_int texture_size;
	TextureFormat texture_format{ default_texture_format };
	Rect rect;
	Transform transform;
	std::array<V2_float, 4> texture_coordinates{ GetDefaultTextureCoordinates() };
	Origin origin{ default_origin };
	Depth depth;
	Tint tint;
	PreFX pre_fx;
	RenderState render_state;
};

struct DrawShaderCommand {
	// How subsequent shader calls are blended to the intermediate target.
	BlendMode intermediate_blend_mode{ default_blend_mode };
	// How the intermediate target is blended to the drawing target. If unset, uses the drawing
	// target's blend mode.
	std::optional<BlendMode> target_blend_mode{ std::nullopt };
	bool clear_between_consecutive_calls{ true };
	TextureFormat texture_format{ default_texture_format };
	// If V2_int{} uses drawing to render target viewport size. If Texture, uses texture size.
	TextureOrSize texture_or_size{ V2_int{} };
	Color target_clear_color{ color::Transparent };
	Depth depth;
	// Entity passed to render_state.shader_pass.uniform_callback. Can be {}.
	Entity entity;
	RenderState render_state;
};

struct EnableStencilMask {};

struct DisableStencilMask {};

struct DrawOutsideStencilMask {};

struct DrawInsideStencilMask {};

using DrawCommand = std::variant<
	DrawShapeCommand, DrawLinesCommand, DrawTextureCommand, DrawShaderCommand, EnableStencilMask,
	DisableStencilMask, DrawInsideStencilMask, DrawOutsideStencilMask>;

inline constexpr float min_line_width{ 1.0f };
inline constexpr std::array<Index, 6> quad_indices{ 0, 1, 2, 2, 3, 0 };
inline constexpr std::array<Index, 3> triangle_indices{ 0, 1, 2 };

[[nodiscard]] static constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
	return {
		V2_float{ 0.0f, 0.0f },
		V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ 0.0f, 1.0f },
	};
}

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const V2_float& source_position, const V2_float& source_size, const V2_float& texture_size,
	bool offset_texels = false
);

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);

} // namespace impl

class Renderer {
public:
	Renderer() = delete;
	Renderer(Window& window);

	~Renderer() noexcept					 = default;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	void SetBackgroundColor(const Color& background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

	// @param game_size Setting to {} will use window size.
	void SetGameSize(
		const V2_int& game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	// @return The display size of the renderer.
	[[nodiscard]] V2_int GetDisplaySize() const;

	// @return The amount by which game size is scaled to achieve the display size.
	[[nodiscard]] V2_float GetScale() const;

	// @return The game size of the renderer.
	[[nodiscard]] V2_int GetGameSize() const;

	// @return The game size scaling mode.
	[[nodiscard]] ScalingMode GetScalingMode() const;

	void DrawTexture(
		const impl::Texture& texture, const Transform& transform, const V2_float& texture_size = {},
		Origin origin = default_origin, const Tint& tint = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PreFX& pre_fx = {}, const PostFX& post_fx = {},
		const std::array<V2_float, 4>& texture_coordinates = impl::GetDefaultTextureCoordinates()
	);

	void DrawTexture(
		const TextureHandle& texture_key, const Transform& transform,
		const V2_float& texture_size = {}, Origin origin = default_origin, const Tint& tint = {},
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PreFX& pre_fx = {}, const PostFX& post_fx = {},
		const std::array<V2_float, 4>& texture_coordinates = impl::GetDefaultTextureCoordinates()
	);

	void DrawLines(
		const Transform& transform, const std::vector<V2_float>& line_points, const Tint& color,
		const LineWidth& line_width = {}, bool connect_last_to_first = false,
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PostFX& post_fx = {}
	);

	void DrawLines(
		const std::vector<V2_float>& line_points, const Tint& color,
		const LineWidth& line_width = {}, bool connect_last_to_first = false,
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PostFX& post_fx = {}
	);

	// @param origin only applicable to Rect and RoundedRect.
	void DrawShape(
		const Transform& transform, const Shape& shape, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = default_origin, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}, const impl::ShaderPass& shader_pass = {}
	);

	void DrawShader(
		const impl::ShaderPass& shader, const Entity& entity,
		bool clear_between_consecutive_calls	   = true,
		const Color& target_clear_color			   = color::Transparent,
		const impl::TextureOrSize& texture_or_size = V2_int{},
		BlendMode intermediate_blend_mode = default_blend_mode, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		TextureFormat texture_format = default_texture_format, const PostFX& post_fx = {},
		std::optional<BlendMode> target_blend_mode = std::nullopt
	);

	// @param text_size {} results in unscaled size of text based on font.
	void DrawText(
		const std::string& content, Transform transform, const TextColor& color,
		Origin origin = default_origin, const FontSize& font_size = {},
		const ResourceHandle& font_key = {}, const TextProperties& properties = {},
		V2_float text_size = {}, const Tint& tint = {}, bool hd_text = true,
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PreFX& pre_fx = {}, const PostFX& post_fx = {},
		const std::array<V2_float, 4>& texture_coordinates = impl::GetDefaultTextureCoordinates()
	);

	void DrawRect(
		const Transform& transform, const Rect& rect, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = default_origin, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawRoundedRect(
		const Transform& transform, const RoundedRect& rounded_rect, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = default_origin, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawLine(
		const Transform& transform, const Line& line, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawLine(
		const V2_float& start, const V2_float& end, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawTriangle(
		const Transform& transform, const Triangle& triangle, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawEllipse(
		const Transform& transform, const Ellipse& ellipse, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawCircle(
		const Transform& transform, const Circle& circle, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawCapsule(
		const Transform& transform, const Capsule& capsule, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawArc(
		const Transform& transform, const Arc& arc, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawPolygon(
		const Transform& transform, const Polygon& polygon, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawPoint(
		const V2_float& point, const Tint& color, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {}
	);

	void EnableStencilMask();
	void DisableStencilMask();
	void DrawOutsideStencilMask();
	void DrawInsideStencilMask();

	// TODO: figure out if this should be public or private.
	void Submit(const impl::DrawCommand& command, bool debug = false);

private:
	friend struct impl::ViewportResizeScript;

	void AddTemporaryTexture(Texture&& texture);

	[[nodiscard]] std::size_t GetMaxTextureSlots() const;

	void DrawCommand(const impl::DrawCommand& cmd);
	void DrawLines(const impl::DrawLinesCommand& cmd);
	void DrawTexture(const impl::DrawTextureCommand& cmd);
	void DrawShader(const impl::DrawShaderCommand& cmd);

	void AddLinesImpl(
		std::span<impl::Vertex> line_vertices, std::span<const Index> line_indices,
		std::span<const V2_float> points, float line_width, const Transform& transform
	);

	void AddVertices(
		std::span<const impl::Vertex> point_vertices, std::span<const Index> point_indices
	);

	static void InvokeDrawable(const Entity& entity);
	static void InvokeDrawFilter(RenderTarget& render_target, FilterType type);

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
	[[nodiscard]] impl::TextureId PingPong(
		const std::vector<Entity>& container,
		const std::shared_ptr<impl::DrawContext>& read_context, impl::TextureId id,
		impl::DrawTarget target, bool flip_vertices
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

	[[nodiscard]] const Shader& GetCurrentShader() const;

	// @return True if the render state changed, false otherwise.
	bool SetState(const RenderState& new_render_state);

	void RecomputeDisplaySize(const V2_int& window_size);

	void Flush(bool final_flush = false);

	void DrawScene(Scene& scene);

	void Draw(Scene& scene);

	// Draws the screen target to the default frame buffer.
	void DrawScreenTarget();

	// @param filter If function returns true, the entity is not drawn.
	void DrawDisplayList(
		RenderTarget& render_target, std::vector<Entity>& display_list,
		const std::function<bool(const Entity&)>& filter = {}, bool draw_debug = false
	);

	void FlushDrawQueue(TextureId id, bool draw_debug);

	void SetDrawingTo(const RenderTarget& render_target);

	void ClearScreenTarget() const;

	// Clear the scene's internal render target, and all of the render target objects that exist in
	// the scene.
	void ClearRenderTargets(Scene& scene) const;

	Handle<Shader> GetFullscreenShader(bool hdr) const;

	std::vector<impl::DrawCommand> debug_queue_;
	std::unordered_map<TextureId, std::vector<impl::DrawCommand>> draw_queues_;

	std::shared_ptr<DrawContext> intermediate_target;

	DrawTarget drawing_to_;

	// TODO: Clean this up.
	// If true, will flush on the next state change regardless of state being new or not.
	bool force_flush{ false };

	void UpdateResolutions(const V2_int& game_size, ScalingMode scaling_mode);

	bool game_size_set_{ true };
	ScalingMode resolution_mode_{ ScalingMode::Letterbox };

	// Allow for creation of targets before window has been initialized.
	V2_int game_size_{ 1, 1 };
	Viewport display_viewport_{ {}, { 1, 1 } };

	bool game_size_changed_{ true };
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

	[[nodiscard]] impl::Texture CreateTexture(
		Transform& out_transform, V2_float& out_text_size, const TextContent& content,
		const TextColor& color, const FontSize& font_size, const ResourceHandle& font_key,
		const TextProperties& properties, bool hd_text, const Camera& camera
	);

	// Present the screen target to the window.
	void PresentScreen();

	// Clears the window buffer.
	void ClearScreen() const;

	Window& window_;
	std::unique_ptr<impl::gl::GLContext> gl_;
};

PTGN_SERIALIZER_REGISTER_ENUM(
	ScalingMode, { { ScalingMode::Disabled, "disabled" },
				   { ScalingMode::Stretch, "stretch" },
				   { ScalingMode::Letterbox, "letterbox" },
				   { ScalingMode::Overscan, "overscan" },
				   { ScalingMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn