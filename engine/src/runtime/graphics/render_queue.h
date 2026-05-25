#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_command.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn {

class Scene;
class SceneContext;
class DrawContext;
class Rect;
class RoundedRect;
class Triangle;
class Polygon;
class Line;
class Circle;
class Ellipse;
class Capsule;
class Arc;
class Shape;
class Renderer;

namespace impl {

struct CameraRenderCommands {
	RenderCamera camera;
	RenderCommands commands;
};

struct EntityRenderCommand {
	Entity entity;
	float depth{ 0.0f };
};

struct CameraEntityCommands {
	RenderCamera camera;
	std::vector<EntityRenderCommand> commands;
};

struct CameraRenderBucket {
	const RenderCamera* camera{ nullptr };

	std::vector<EntityRenderCommand>* entity_commands{ nullptr };
	RenderCommands* manual_commands{ nullptr };
};

struct ClearedEntities {
	std::vector<RenderTarget> render_targets;
	std::vector<CameraUUID> cameras;
};

} // namespace impl

struct TextureRenderParams {
	std::optional<V2_float> size;
	Origin origin{ Origin::Center };
	Color tint{ color::White };
	Depth depth;
	std::optional<BlendMode> blend_mode;
	std::optional<std::array<V2_float, 4>> texture_coordinates;
	std::optional<impl::RenderCamera> camera;
	int entity_id{ -1 };
};

struct ShapeRenderParams {
	FillStyle fill_style{ 1.0f };
	Origin origin{ Origin::Center };
	Depth depth;
	std::optional<BlendMode> blend_mode;
	std::optional<impl::RenderCamera> camera;
	int entity_id{ -1 };
	/// @brief If true, the shape will be drawn to the debug layer which is drawn last.
	bool debug{ false };
};

class RenderQueue {
public:
	void DrawTexture(
		Transform transform, std::string_view texture_key, TextureRenderParams params = {}
	);

	void DrawTexture(
		Transform transform, std::string_view texture_key, std::string_view shader_key,
		TextureRenderParams params = {}
	);

	/// @param params If size is nullopt, uses the entire game size.
	void DrawShader(
		Transform transform, std::string_view shader_key, TextureRenderParams params = {}
	);

	void DrawPoint(V2_float point, Color color, ShapeRenderParams params = {});

	void DrawLine(
		V2_float start, V2_float end, Color color,
		ShapeRenderParams params = ShapeRenderParams{ .fill_style{ 1.0f } }
	);

	void DrawLines(
		std::span<const V2_float> points, Color color,
		ShapeRenderParams params = ShapeRenderParams{ .fill_style{ 1.0f } }, bool closed = false,
		std::optional<Transform> transform = std::nullopt
	);

	void DrawShape(
		Transform transform, const V2_float& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Rect& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const RoundedRect& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Polygon& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Triangle& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Capsule& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Line& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Arc& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Circle& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Ellipse& shape, Color color, ShapeRenderParams params = {}
	);

	void DrawShape(
		Transform transform, const Shape& shape, Color color, ShapeRenderParams params = {}
	);

	// TODO: Fix.
	// void DrawText(
	//	std::string_view text_content, Transform transform, Color text_color,
	//	FontSize font_size = {}, FontOrKey font = {}, const TextProperties& properties = {},
	//	Origin draw_origin = Origin::Center, std::optional<V2_float> text_size = {},
	//	Depth depth = {}, std::optional<BlendMode> blend_mode = {},
	//	const std::optional<SceneCamera>& camera = {}, int entity_id = -1
	//);

private:
	friend class Scene;
	friend class SceneContext;

	void DrawTexture(
		Transform transform, impl::TextureId texture, V2_int texture_size, impl::ShaderId shader,
		TextureRenderParams params
	);

	impl::ShaderId GetShader(std::string_view shader_key) const;

	RenderQueue() = delete;
	RenderQueue(Scene& scene, Renderer& renderer);
	~RenderQueue() noexcept						   = default;
	RenderQueue(const RenderQueue&)				   = delete;
	RenderQueue& operator=(const RenderQueue&)	   = delete;
	RenderQueue(RenderQueue&&) noexcept			   = default;
	RenderQueue& operator=(RenderQueue&&) noexcept = delete;

	/// @brief If a primary world camera is set, we combine all debug commands into a single command
	/// list for that camera.
	void CombineDebugCommands(const impl::RenderCamera& camera);

	void Draw(
		DrawContext& ctx, const RenderTarget& scene_render_target, impl::ClearedEntities& cleared,
		V2_int game_size, const std::vector<impl::CameraRenderBucket>& buckets
	);

	void Draw(
		DrawContext& ctx, const RenderTarget& scene_render_target, impl::ClearedEntities& cleared,
		V2_int game_size, const impl::CameraRenderBucket& bucket
	);

	void SetupCamera(
		const RenderTarget& scene_render_target, impl::ClearedEntities& cleared, V2_int game_size,
		const impl::RenderCamera& render_camera
	);

	static std::vector<impl::CameraRenderBucket> GetRenderBuckets(
		std::vector<impl::CameraRenderCommands>& manual_commands,
		std::vector<impl::CameraEntityCommands>& entity_commands
	);

	/// @brief If camera is {}, returns draw commands for the primary scene camera. If draw commands
	/// do not exist for the camera, adds them to the vector.
	impl::RenderCommands& GetRenderCommands(
		const std::optional<impl::RenderCamera>& camera, bool debug
	);

	std::vector<impl::CameraRenderCommands> render_commands_;
	std::vector<impl::CameraRenderCommands> debug_commands_;

	std::vector<impl::TextureObject> temporary_textures_;

	Scene& scene_;
	Renderer& renderer_;

	std::optional<BlendMode> debug_blend_mode;
};

} // namespace ptgn