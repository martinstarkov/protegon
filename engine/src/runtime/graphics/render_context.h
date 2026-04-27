#pragma once

#include <array>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene_camera.h"

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
	/// @param game_size Setting to nullopt will dynamically use the presentation viewport size.
	void SetGameSize(
		std::optional<V2_int> game_size = std::nullopt,
		ScalingMode scaling_mode		= ScalingMode::Letterbox
	);

	/// @param scaling_mode The method by which the game size is scaled to fit the presentation
	/// viewport.
	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @param presentation_viewport Setting to nullopt will use full window size.
	/// Viewport position should be relative to the window top left.
	void SetPresentationViewport(std::optional<Viewport> presentation_viewport = std::nullopt);

	/// @return True if the game size is set, false otherwise.
	bool HasGameSize() const;

	/// @return The game size of the renderer. Returns presentation viewport size if unset.
	V2_int GetGameSize() const;

	/// @return The method by which the game size is scaled to fit the presentation
	/// viewport.
	ScalingMode GetScalingMode() const;

	/// @return The presentation viewport with position relative to the window top left.
	/// Returns a viewport covering the entire window if unset.
	Viewport GetPresentationViewport() const;

	/// @return The presentation viewport position relative to the window top left. Returns {0, 0}
	/// if unset.
	V2_int GetPresentationPosition() const;

	/// @return The presentation viewport of the renderer. If unset, returns the window size.
	V2_int GetPresentationSize() const;

	/// @brief The display viewport is the area inside presentation rectangle that the game is
	/// rendered to. It is defined by scaling the game size to fit inside the presentation viewport
	/// according to the scaling mode.
	/// The position of the display viewport is relative to the top left of the window.
	Viewport GetDisplayViewport() const;

	/// @return The display position of the renderer.
	V2_int GetDisplayPosition() const;

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The size of the entire viewport that the presentation viewport is within. This is
	/// always equal to the window size.
	V2_int GetFullViewportSize() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	void DrawTexture(
		std::string_view texture_key, Transform transform,
		std::optional<V2_float> size = std::nullopt, Origin draw_origin = Origin::Center,
		std::optional<Color> tint = std::nullopt, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = std::nullopt,
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	void DrawTexture(
		std::string_view texture_key, std::string_view shader_key, Transform transform,
		std::optional<V2_float> size = std::nullopt, Origin draw_origin = Origin::Center,
		std::optional<Color> tint = std::nullopt, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = std::nullopt,
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	/// @param size If size is nullopt, uses the entire game size.
	void DrawShader(
		std::string_view shader_key, Transform transform,
		std::optional<V2_float> size = std::nullopt, Origin draw_origin = Origin::Center,
		std::optional<Color> tint = std::nullopt, Depth depth = {},
		std::optional<BlendMode> blend_mode		 = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	void DrawLines(
		const std::vector<V2_float>& points, Color color, float line_width = kMinLineWidth,
		bool connect_last_to_first = false, std::optional<Transform> transform = std::nullopt,
		Depth depth = {}, std::optional<BlendMode> blend_mode = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	void DrawShape(
		Transform transform, const Shape& shape, Color color, FillStyle fill_style,
		Origin draw_origin = Origin::Center, Depth depth = {},
		std::optional<BlendMode> blend_mode		 = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	// TODO: Fix.
	// void DrawText(
	//	std::string_view text_content, Transform transform, Color text_color,
	//	FontSize font_size = {}, FontOrKey font = {}, const TextProperties& properties = {},
	//	Origin draw_origin = Origin::Center, std::optional<V2_float> text_size = {},
	//	Depth depth = {}, std::optional<BlendMode> blend_mode = {},
	//	const std::optional<SceneCamera>& camera = {}, int entity_id = -1
	//);

	void DrawLine(
		V2_float start, V2_float end, Color color, float line_width = kMinLineWidth,
		Depth depth = {}, std::optional<BlendMode> blend_mode = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	void DrawPoint(
		V2_float point, Color color, Depth depth = {},
		std::optional<BlendMode> blend_mode		 = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	const std::optional<Camera>& GetPrimaryWorldCamera() const;

	void DrawTexture(
		impl::TextureId texture, V2_int texture_size, impl::ShaderId shader, Transform transform,
		std::optional<V2_float> size = std::nullopt, Origin draw_origin = Origin::Center,
		std::optional<Color> tint = std::nullopt, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = std::nullopt,
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = std::nullopt,
		const std::optional<SceneCamera>& camera = std::nullopt, int entity_id = -1
	);

	impl::ShaderId GetShader(std::string_view shader_key) const;

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

	void SetPrimaryWorldCamera(const std::optional<Camera>& camera = std::nullopt);

	template <typename T, typename R>
	static void AddDrawCommand(T& commands, const R& command, float depth) {
		if constexpr (std::ranges::input_range<R>) {
			for (const auto& c : command) {
				commands.emplace_back(c, depth);
			}
		} else {
			commands.emplace_back(command, depth);
		}
	}

	/// @brief If camera is {}, returns draw commands for the primary scene camera. If draw commands
	/// do not exist for the camera, adds them to the vector.
	std::vector<impl::DrawCommand>& GetDrawCommandsForCamera(
		const std::optional<impl::RenderCamera>& camera
	);
	std::vector<impl::ManualDrawCommand>& GetDebugCommandsForCamera(
		const std::optional<impl::RenderCamera>& camera
	);

	Scene& scene_;
	impl::Renderer& renderer_;

	/// @brief Keys are uuids of cameras.
	std::vector<std::pair<impl::RenderCamera, std::vector<impl::DrawCommand>>> draw_commands_;
	std::vector<std::pair<impl::RenderCamera, std::vector<impl::ManualDrawCommand>>>
		debug_commands_;

	std::vector<impl::TextureObject> temporary_textures_;
};

} // namespace ptgn