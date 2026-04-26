#include "runtime/graphics/render_context.h"

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/vertex/vertex.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

RenderContext::RenderContext(Scene& scene, impl::Renderer& renderer) :
	scene_{ scene }, renderer_{ renderer } {}

std::vector<impl::DrawCommand>& RenderContext::GetDrawCommandsForCamera(
	const std::optional<impl::RenderCamera>& camera
) {
	impl::RenderCamera cam;

	if (camera.has_value()) {
		cam = *camera;
	} else {
		cam = impl::RenderCamera{ scene_.ctx().camera };
	}

	for (auto& [c, commands] : draw_commands_) {
		if (c == cam) {
			return commands;
		}
	}
	return draw_commands_.emplace_back(cam, std::vector<impl::DrawCommand>{}).second;
}

std::vector<impl::ManualDrawCommand>& RenderContext::GetDebugCommandsForCamera(
	const std::optional<impl::RenderCamera>& camera
) {
	impl::RenderCamera cam;

	if (camera.has_value()) {
		cam = *camera;
	} else {
		cam = impl::RenderCamera{ scene_.ctx().camera };
	}

	for (auto& [c, commands] : debug_commands_) {
		if (c == cam) {
			return commands;
		}
	}
	return debug_commands_.emplace_back(cam, std::vector<impl::ManualDrawCommand>{}).second;
}

void RenderContext::DrawTexture(
	impl::TextureId texture, V2_int texture_size, impl::ShaderId shader, Transform transform,
	std::optional<V2_float> size, Origin draw_origin, std::optional<Color> tint, Depth depth,
	std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera.transform([](const auto& c) {
		return impl::RenderCamera{ c };
	})) };

	Rect rect{ size.value_or(V2_float{ texture_size }) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	std::array<V2_float, 4> tex_coords{
		texture_coordinates.value_or(impl::GetDefaultTextureCoordinates<false>())
	};

	impl::TextureCommand texture_command{
		shader, texture, positions, tint.value_or(color::White), tex_coords, blend_mode, entity_id
	};

	draw_commands.emplace_back(texture_command, depth);
}

void RenderContext::DrawTexture(
	std::string_view texture_key, Transform transform, std::optional<V2_float> size,
	Origin draw_origin, std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	auto texture_shader{ renderer_.GetShader("texture") };

	const auto& assets{ scene_.ctx().asset };

	auto texture{ assets.Get<Texture>(texture_key) };
	auto texture_size{ texture.GetSize() };

	DrawTexture(
		texture, texture_size, texture_shader, transform, size, draw_origin, tint, depth,
		blend_mode, texture_coordinates, camera, entity_id
	);
}

void RenderContext::DrawTexture(
	std::string_view texture_key, std::string_view shader_key, Transform transform,
	std::optional<V2_float> size, Origin draw_origin, std::optional<Color> tint, Depth depth,
	std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	const auto& assets{ scene_.ctx().asset };
	auto texture{ assets.Get<Texture>(texture_key) };
	auto shader{ assets.Get<Shader>(shader_key) };
	auto texture_size{ texture.GetSize() };

	DrawTexture(
		texture, texture_size, shader, transform, size, draw_origin, tint, depth, blend_mode,
		texture_coordinates, camera, entity_id
	);
}

void RenderContext::DrawShader(
	std::string_view shader_key, Transform transform, std::optional<V2_float> size,
	Origin draw_origin, std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	const auto& assets{ scene_.ctx().asset };
	auto shader{ assets.Get<Shader>(shader_key) };

	auto& draw_commands{ GetDrawCommandsForCamera(camera.transform([](const auto& c) {
		return impl::RenderCamera{ c };
	})) };

	Rect rect{ size.value_or(renderer_.GetGameSize()) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	impl::QuadCommand quad_command{ shader, positions, tint.value_or(color::White), blend_mode,
									entity_id };

	draw_commands.emplace_back(quad_command, depth);
}

void RenderContext::DrawLines(
	const std::vector<V2_float>& points, Color color, float line_width, bool connect_last_to_first,
	std::optional<Transform> transform, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	auto& camera_commands{ GetDrawCommandsForCamera(camera.transform([](const auto& c) {
		return impl::RenderCamera{ c };
	})) };

	auto draw_commands{ DrawContext::GetDrawCommand(
		renderer_.GetShader("color"), points, line_width, transform.value_or(Transform{}), color,
		blend_mode, connect_last_to_first, entity_id
	) };

	PTGN_ASSERT(std::holds_alternative<std::vector<impl::QuadCommand>>(*draw_commands));

	const auto& line_draw_commands{ std::get<std::vector<impl::QuadCommand>>(*draw_commands) };

	for (const auto& line_command : line_draw_commands) {
		camera_commands.emplace_back(line_command, depth);
	}
}

void RenderContext::DrawShape(
	Transform transform, const Shape& shape, Color color, FillStyle fill_style, Origin draw_origin,
	Depth depth, std::optional<BlendMode> blend_mode, const std::optional<SceneCamera>& camera,
	int entity_id
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera.transform([](const auto& c) {
		return impl::RenderCamera{ c };
	})) };

	auto shape_draw_commands{ DrawContext::GetDrawCommand(
		renderer_, shape, transform, color, fill_style, draw_origin, blend_mode, entity_id
	) };

	if (!shape_draw_commands.has_value()) {
		return;
	}

	std::visit(
		[&](const auto& cmd) { AddDrawCommand(draw_commands, cmd, depth); }, *shape_draw_commands
	);
}

void RenderContext::DrawLine(
	V2_float start, V2_float end, Color color, float line_width, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<SceneCamera>& camera, int entity_id
) {
	DrawShape(
		Transform{}, Line{ start, end }, color, line_width, Origin::Center, depth, blend_mode,
		camera, entity_id
	);
}

void RenderContext::DrawPoint(
	V2_float point, Color color, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	DrawShape(
		Transform{}, point, color, Solid{}, Origin::Center, depth, blend_mode, camera, entity_id
	);
}

// TODO: Fix.
// void RenderContext::DrawText(
//	std::string_view text_content, Transform transform, Color text_color, FontSize font_size,
//	FontOrKey font, const TextProperties& properties, Origin draw_origin,
//	std::optional<V2_float> text_size, Depth depth, std::optional<BlendMode> blend_mode,
//	const std::optional<SceneCamera>& camera, int entity_id
//) {
//	auto texture_object{ scene_.ctx().asset.CreateTextTextureObject(
//		text_content, text_color, font_size, font, properties
//	) };
//
//	if (!texture_object.has_value()) {
//		return;
//	}
//
//	auto texture_size{ texture_object->GetSize() };
//
//	auto texture_id{ texture_object->operator impl::TextureId() };
//
//	temporary_textures_.emplace_back(std::move(*texture_object));
//
//	auto texture_shader{ renderer_.GetShader("texture") };
//
//	DrawTexture(
//		texture_id, texture_size, texture_shader, transform, text_size, draw_origin, color::White,
//		depth, blend_mode, {}, camera, entity_id
//	);
// }

void RenderContext::SetGameSize(std::optional<V2_int> game_size, ScalingMode scaling_mode) {
	renderer_.SetGameSize(game_size, scaling_mode);
}

void RenderContext::SetScalingMode(ScalingMode scaling_mode) {
	renderer_.SetScalingMode(scaling_mode);
}

void RenderContext::SetPresentationViewport(std::optional<Viewport> presentation_viewport) {
	renderer_.SetPresentationViewport(presentation_viewport);
}

bool RenderContext::HasGameSize() const {
	return renderer_.HasGameSize();
}

V2_int RenderContext::GetGameSize() const {
	return renderer_.GetGameSize();
}

ScalingMode RenderContext::GetScalingMode() const {
	return renderer_.GetScalingMode();
}

Viewport RenderContext::GetPresentationViewport() const {
	return renderer_.GetPresentationViewport();
}

V2_int RenderContext::GetPresentationPosition() const {
	return renderer_.GetPresentationPosition();
}

V2_int RenderContext::GetPresentationSize() const {
	return renderer_.GetPresentationSize();
}

Viewport RenderContext::GetDisplayViewport() const {
	return renderer_.GetDisplayViewport();
}

V2_int RenderContext::GetDisplayPosition() const {
	return renderer_.GetDisplayPosition();
}

V2_int RenderContext::GetDisplaySize() const {
	return renderer_.GetDisplaySize();
}

V2_float RenderContext::GetScale() const {
	return renderer_.GetScale();
}

V2_int RenderContext::GetFullViewportSize() const {
	return renderer_.GetFullViewportSize();
}

void RenderContext::SetBackgroundColor(Color background_color) {
	renderer_.SetBackgroundColor(background_color);
}

Color RenderContext::GetBackgroundColor() const {
	return renderer_.GetBackgroundColor();
}

void RenderContext::SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera) {
	renderer_.SetPrimaryWorldCamera(primary_world_camera);
}

const std::optional<Camera>& RenderContext::GetPrimaryWorldCamera() const {
	return renderer_.GetPrimaryWorldCamera();
}

} // namespace ptgn