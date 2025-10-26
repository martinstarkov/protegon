#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/app/manager.h"
#include "core/app/resolution.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/effects.h"
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
#include "renderer/api/vertex.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/buffers/vertex_array.h"
#include "renderer/materials/texture.h"
#include "renderer/render_target.h"
#include "serialization/json/enum.h"
#include "serialization/json/serializable.h"

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

constexpr BlendMode default_blend_mode{ BlendMode::Blend };
constexpr Origin default_origin{ Origin::Center };
constexpr TextureFormat default_texture_format{ TextureFormat::RGBA8888 };

namespace impl {

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

	ShaderPass(const Shader& shader, const UniformCallback& uniform_callback = nullptr);

	ShaderPass(std::string_view shader_name, const UniformCallback& uniform_callback = nullptr);

	ShaderPass(const char* shader_name);

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

class RenderData {
public:
	void Submit(const DrawCommand& command, bool debug = false);

	void AddTemporaryTexture(Texture&& texture);

	[[nodiscard]] std::size_t GetMaxTextureSlots() const;

	void DrawCommand(const impl::DrawCommand& cmd);
	void DrawLines(const DrawLinesCommand& cmd);
	void DrawTexture(const DrawTextureCommand& cmd);
	void DrawShader(const DrawShaderCommand& cmd);

	void AddLinesImpl(
		std::span<Vertex> line_vertices, std::span<const Index> line_indices,
		std::span<const V2_float> points, float line_width, const Transform& transform
	);

	void AddVertices(std::span<const Vertex> point_vertices, std::span<const Index> point_indices);

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
	[[nodiscard]] TextureId PingPong(
		const std::vector<Entity>& container, const std::shared_ptr<DrawContext>& read_context,
		TextureId id, DrawTarget target, bool flip_vertices
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

	static const Shader& GetFullscreenShader(TextureFormat texture_format);

	std::vector<impl::DrawCommand> debug_queue_;
	std::unordered_map<TextureId, std::vector<impl::DrawCommand>> draw_queues_;

	std::shared_ptr<DrawContext> intermediate_target;

	DrawTarget drawing_to_;

	// TODO: Clean this up.
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