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
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/text/text_layout.h"
#include "runtime/ecs/entity.h"
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
struct StyledText;

namespace impl {

struct EntityRenderCommand {
	Entity entity;
	Depth depth;
};

struct CameraRenderCommands {
	SceneCamera camera;
	RenderCommands commands;
};

struct CameraEntityCommands {
	SceneCamera camera;
	std::vector<EntityRenderCommand> commands;
};

} // namespace impl

struct TextureRenderParams {
	/// @brief If nullopt, uses the texture size.
	std::optional<V2_float> size;
	Origin origin{ Origin::Center };
	Color tint{ color::White };
	Depth depth;
	std::optional<BlendMode> blend_mode;
	std::optional<std::array<V2_float, 4>> texture_coordinates;
	std::optional<SceneCamera> camera;
	int entity_id{ impl::kNoEntityId };
};

struct ShapeRenderParams {
	FillStyle fill_style{ 1.0f };
	Origin origin{ Origin::Center };
	Depth depth;
	std::optional<BlendMode> blend_mode;
	std::optional<SceneCamera> camera;
	int entity_id{ impl::kNoEntityId };
	/// @brief If true, the shape will be drawn to the debug layer which is drawn last.
	bool debug{ false };
};

struct TextRenderParams {
	Origin origin{ Origin::Center };
	Color tint{ color::White };
	Depth depth;
	std::optional<BlendMode> blend_mode;
	std::optional<SceneCamera> camera;
	int entity_id{ impl::kNoEntityId };
	/// @brief If true, the text will be drawn to the debug layer which is drawn last.
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

	void DrawShader(
		Transform transform, std::string_view shader_key, TextureRenderParams params = {}
	);

	void DrawText(
		Transform transform, std::string_view text, Color color, float font_size,
		const TextBox& text_box = {}, TextRenderParams params = {}
	);

	void DrawText(
		Transform transform, StyledText styled_text, TextBox text_box = {},
		TextRenderParams params = {}
	);

	void DrawPoint(V2_float point, Color color, ShapeRenderParams params = {});

	void DrawLine(
		V2_float start, V2_float end, Color color,
		ShapeRenderParams params = ShapeRenderParams{ .fill_style{ 1.0f } },
		Transform transform		 = {}
	);

	void DrawLines(
		std::span<const V2_float> points, Color color,
		ShapeRenderParams params = ShapeRenderParams{ .fill_style{ 1.0f } }, bool closed = false,
		Transform transform = {}
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

	void DrawTexture(
		Transform transform, impl::TextureId texture, V2_int texture_size, impl::ShaderId shader,
		TextureRenderParams params = {}
	);

private:
	friend class Scene;
	friend class SceneContext;

	RenderQueue() = delete;
	RenderQueue(Renderer& renderer, Scene& parent_scene);
	~RenderQueue() noexcept						   = default;
	RenderQueue(const RenderQueue&)				   = delete;
	RenderQueue& operator=(const RenderQueue&)	   = delete;
	RenderQueue(RenderQueue&&) noexcept			   = delete;
	RenderQueue& operator=(RenderQueue&&) noexcept = delete;

	void Rebind(Scene& parent_scene);

	impl::ShaderId GetShader(std::string_view shader_key) const;

	/// @brief If a primary world camera is set, combine all commands into a single command
	/// list for that camera.
	void CombineCommands();

	/// @brief If camera is {}, returns draw commands for the primary scene camera. If draw commands
	/// do not exist for the camera, adds them to the vector.
	impl::RenderCommands& GetRenderCommands(std::optional<SceneCamera> camera, bool debug);

	std::vector<impl::CameraRenderCommands> render_commands_;
	std::vector<impl::CameraRenderCommands> debug_commands_;

	Scene* scene_{ nullptr };
	Renderer& renderer_;
};

} // namespace ptgn