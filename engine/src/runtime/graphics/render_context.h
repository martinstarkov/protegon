#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"

namespace ptgn {

class Scene;
class SceneContext;
class RenderContext;
class DebugContext;

namespace impl {

class Renderer;

struct DrawCommand {
	std::variant<Entity, ManualCommand> payload;
	float depth{ 0.0f };
};

} // namespace impl

class RenderContext {
public:
	/// @param game_size Setting to {} will use dynamic window size.
	void SetGameSize(
		std::optional<V2_int> game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	Viewport GetDisplayViewport() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The game size of the renderer. Returns window size if unset.
	V2_int GetGameSize() const;

	/// @return The game size scaling mode.
	ScalingMode GetScalingMode() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	void DrawTexture(
		TextureOrKey texture, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = {},
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = {},
		const std::optional<Camera>& camera								  = {}
	);

	void DrawTexture(
		TextureOrKey texture, Shader shader, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = {},
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = {},
		const std::optional<Camera>& camera								  = {}
	);

	/// @param size If size is {}, uses the entire game size.
	/// @param user_data Optional array of 4 floats that can be used to pass per vertex data to the
	/// shader.
	void DrawShader(
		Shader shader, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {},
		const std::optional<std::array<float, 4>>& user_data = {}
	);

	void DrawLines(
		const std::vector<V2_float>& points, Color color, float line_width = kMinLineWidth,
		bool connect_last_to_first = false, std::optional<Transform> transform = {},
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawShape(
		const Shape& shape, Transform transform, Color color, FillStyle fill_style,
		Origin draw_origin = Origin::Center, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawText(
		std::string_view text_content, Transform transform, Color text_color,
		FontSize font_size = {}, FontOrKey font = {}, const TextProperties& properties = {},
		Origin draw_origin = Origin::Center, std::optional<V2_float> text_size = {},
		bool hd_text = true, Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawRect(
		Transform transform, const Rect& rect, Color color, FillStyle fill_style = 1.0f,
		Origin draw_origin = Origin::Center, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawRoundedRect(
		Transform transform, const RoundedRect& rounded_rect, Color color,
		FillStyle fill_style = 1.0f, Origin draw_origin = Origin::Center, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawLine(
		Transform transform, const Line& line, Color color, float line_width = kMinLineWidth,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawLine(
		V2_float start, V2_float end, Color color, float line_width = kMinLineWidth,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawTriangle(
		Transform transform, const Triangle& triangle, Color color, FillStyle fill_style = 1.0f,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawEllipse(
		Transform transform, const Ellipse& ellipse, Color color, FillStyle fill_style = 1.0f,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawCircle(
		Transform transform, const Circle& circle, Color color, FillStyle fill_style = 1.0f,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawCapsule(
		Transform transform, const Capsule& capsule, Color color, FillStyle fill_style = 1.0f,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawArc(
		Transform transform, const Arc& arc, Color color, FillStyle fill_style = 1.0f,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawPolygon(
		Transform transform, const Polygon& polygon, Color color, FillStyle fill_style = 1.0f,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawPoint(
		V2_float point, Color color, Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

private:
	friend class Scene;
	friend class SceneContext;
	friend class DebugContext;

	RenderContext() = delete;
	RenderContext(Scene& scene, impl::Renderer& renderer);
	~RenderContext() noexcept						   = default;
	RenderContext(const RenderContext&)				   = delete;
	RenderContext& operator=(const RenderContext&)	   = delete;
	RenderContext(RenderContext&&) noexcept			   = default;
	RenderContext& operator=(RenderContext&&) noexcept = delete;

	void DrawTexture(
		impl::TextureId texture, V2_int texture_size, impl::ShaderId shader, Transform transform,
		std::optional<V2_float> size, Origin draw_origin, std::optional<Color> tint, Depth depth,
		std::optional<BlendMode> blend_mode,
		const std::optional<std::array<V2_float, 4>>& texture_coordinates,
		const std::optional<Camera>& camera
	);

	template <typename T, typename R>
	static void AddDrawCommand(T& commands, const R& command, float depth) {
		if constexpr (SpecializationOf<R, std::vector>) {
			for (const auto& c : command) {
				commands.emplace_back(c, depth);
			}
		} else {
			commands.emplace_back(command, depth);
		}
	}

	/// @brief If camera is {}, returns draw commands for the primary scene camera. If draw commands
	/// do not exist for the camera, adds them to the vector.
	std::vector<impl::DrawCommand>& GetDrawCommandsForCamera(const std::optional<Camera>& camera);
	std::vector<impl::ManualDrawCommand>& GetDebugCommandsForCamera(
		const std::optional<Camera>& camera
	);

	Scene& scene_;
	impl::Renderer& renderer_;

	std::vector<std::pair<Camera, std::vector<impl::DrawCommand>>> draw_commands_;
	std::vector<std::pair<Camera, std::vector<impl::ManualDrawCommand>>> debug_commands_;

	std::vector<impl::TextureObject> temporary_textures_;
};

} // namespace ptgn