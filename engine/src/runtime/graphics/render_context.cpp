#include "runtime/graphics/render_context.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_command.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

void SortEntityDrawCommands(std::vector<impl::EntityRenderCommand>& commands) {
	std::ranges::stable_sort(commands, [](const auto& a, const auto& b) {
		if (a.depth != b.depth) {
			return a.depth < b.depth;
		}

		return a.entity.WasCreatedBefore(b.entity);
	});
}

} // namespace

RenderContext::RenderContext(Scene& scene, impl::Renderer& renderer) :
	scene_{ scene }, renderer_{ renderer } {}

impl::RenderCommands& RenderContext::GetRenderCommands(
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
	return draw_commands_.emplace_back(cam, impl::RenderCommands{}).commands;
}

impl::RenderCommands& RenderContext::GetDebugRenderCommands(
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
	return debug_commands_.emplace_back(cam, impl::RenderCommands{}).commands;
}

void RenderContext::DrawTexture(
	impl::TextureId texture, V2_int texture_size, impl::ShaderId shader, Transform transform,
	std::optional<V2_float> size, Origin draw_origin, std::optional<Color> tint, Depth depth,
	std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	auto& draw_commands{
		GetRenderCommands(camera.transform([](const auto& c) { return impl::RenderCamera{ c }; }))
	};

	Rect rect{ size.value_or(V2_float{ texture_size }) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	std::array<V2_float, 4> tex_coords{ texture_coordinates.value_or(
		// impl::GetDefaultTextureCoordinates<false>()
		impl::GetTextureCoordinates({}, texture_size, texture_size, false, true)
	) };

	// TODO: Fix.
	// impl::TextureCommand texture_command{
	//	shader, texture, positions, tint.value_or(color::White), tex_coords, blend_mode, entity_id
	//};
	// draw_commands.emplace_back(texture_command, depth);
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

	auto& draw_commands{
		GetRenderCommands(camera.transform([](const auto& c) { return impl::RenderCamera{ c }; }))
	};

	Rect rect{ size.value_or(renderer_.GetGameSize()) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	// TODO: Fix.
	// impl::QuadCommand quad_command{ shader, positions, tint.value_or(color::White), blend_mode,
	//								entity_id };
	// draw_commands.emplace_back(quad_command, depth);
}

void RenderContext::DrawLines(
	const std::vector<V2_float>& points, Color color, float line_width, bool connect_last_to_first,
	std::optional<Transform> transform, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<SceneCamera>& camera, int entity_id
) {
	auto& camera_commands{
		GetRenderCommands(camera.transform([](const auto& c) { return impl::RenderCamera{ c }; }))
	};

	// TODO: Fix.
	// auto draw_commands{ DrawContext::GetDrawCommand(
	//	renderer_.GetShader("color"), points, line_width, transform.value_or(Transform{}), color,
	//	blend_mode, connect_last_to_first, entity_id
	//) };
	// PTGN_ASSERT(std::holds_alternative<std::vector<impl::QuadCommand>>(*draw_commands));
	// const auto& line_draw_commands{ std::get<std::vector<impl::QuadCommand>>(*draw_commands) };
	// for (const auto& line_command : line_draw_commands) {
	//	camera_commands.emplace_back(line_command, depth);
	//}
}

void RenderContext::DrawShape(
	Transform transform, const Shape& shape, Color color, FillStyle fill_style, Origin draw_origin,
	Depth depth, std::optional<BlendMode> blend_mode, const std::optional<SceneCamera>& camera,
	int entity_id
) {
	auto& draw_commands{
		GetRenderCommands(camera.transform([](const auto& c) { return impl::RenderCamera{ c }; }))
	};

	// TODO: Fix.
	// auto shape_draw_commands{ DrawContext::GetDrawCommand(
	//	renderer_, shape, transform, color, fill_style, draw_origin, blend_mode, entity_id
	//) };
	// if (!shape_draw_commands.has_value()) {
	//	return;
	//}
	// std::visit(
	//	[&](const auto& cmd) { AddDrawCommand(draw_commands, cmd, depth); }, *shape_draw_commands
	//);
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

void RenderContext::CombineDebugCommands(const impl::RenderCamera& camera) {
	impl::CameraRenderCommands combined{ .camera = camera };

	for (auto& bucket : debug_commands_) {
		combined.commands.CombineWith(std::move(bucket.commands));
	}

	debug_commands_.clear();
	debug_commands_.emplace_back(std::move(combined));
}

void RenderContext::SetupCamera(
	const RenderTarget& scene_render_target, impl::ClearedEntities& cleared, V2_int game_size,
	const impl::RenderCamera& render_camera
) {
	auto render_target{ render_camera.render_target ? render_camera.render_target
													: scene_render_target };

	renderer_.SetRenderTarget(&render_target.Get<impl::RenderTargetObject>());

	if (bool clear_render_target{ !std::ranges::contains(cleared.render_targets, render_target) };
		clear_render_target) {
		render_target.Clear();
		cleared.render_targets.emplace_back(render_target);
	}

	PTGN_ASSERT(game_size.BothAboveZero(), "Game size dimensions must be above 0");

	auto rt_size{ render_target.GetSize() };
	V2_float scale{ V2_float{ rt_size } / game_size };

	auto viewport{ render_camera.camera.viewport };
	// Not *= because we want float multiplication followed by flooring.
	viewport.position = viewport.position * scale;
	viewport.size	  = viewport.size * scale;
	renderer_.SetViewport(viewport);
	renderer_.SetViewProjection(render_camera.camera.view_projection);

	if (bool clear_camera{ !std::ranges::contains(cleared.cameras, render_camera.uuid) };
		clear_camera && render_camera.clear_color.has_value()) {
		renderer_.SetScissor(ScissorState{ viewport });
		render_target.Clear(*render_camera.clear_color, false);
		renderer_.SetScissor(ScissorState{ false });
		cleared.cameras.emplace_back(render_camera.uuid);
	}
}

void RenderContext::Draw(
	DrawContext& ctx, const RenderTarget& scene_render_target, impl::ClearedEntities& cleared,
	V2_int game_size, const std::vector<impl::CameraRenderBucket>& buckets
) {
	for (const auto& bucket : buckets) {
		Draw(ctx, scene_render_target, cleared, game_size, bucket);
	}
}

void RenderContext::Draw(
	DrawContext& ctx, const RenderTarget& scene_render_target, impl::ClearedEntities& cleared,
	V2_int game_size, const impl::CameraRenderBucket& bucket
) {
	PTGN_ASSERT(bucket.camera);

	SetupCamera(scene_render_target, cleared, game_size, *bucket.camera);

	std::size_t entity_index{ 0 };
	std::size_t manual_index{ 0 };

	if (bucket.entity_commands) {
		SortEntityDrawCommands(*bucket.entity_commands);
	}

	if (bucket.manual_commands) {
		bucket.manual_commands->Sort();
	}

	auto entity_count{ bucket.entity_commands ? bucket.entity_commands->size() : 0 };
	auto manual_count{ bucket.manual_commands ? bucket.manual_commands->Count() : 0 };

	while (entity_index < entity_count || manual_index < manual_count) {
		if (manual_index >= manual_count) {
			PTGN_ASSERT(bucket.entity_commands);
			const auto& entity_cmd{ (*bucket.entity_commands)[entity_index] };
			PTGN_ASSERT(
				IsVisible(entity_cmd.entity), "Cannot render entity without visible component"
			);
			impl::InvokeDrawable(ctx, entity_cmd.entity);
			entity_index++;
			continue;
		}

		if (entity_index >= entity_count) {
			PTGN_ASSERT(bucket.manual_commands);
			bucket.manual_commands->Draw(renderer_, manual_index);
			manual_index++;
			continue;
		}

		auto entity_cmd_depth{ (*bucket.entity_commands)[entity_index].depth };
		auto manual_cmd_depth{ bucket.manual_commands->GetDepth(manual_index) };

		if (NearlyEqual(entity_cmd_depth, manual_cmd_depth) ||
			entity_cmd_depth < manual_cmd_depth) {
			PTGN_ASSERT(bucket.entity_commands);
			const auto& entity_cmd{ (*bucket.entity_commands)[entity_index] };
			PTGN_ASSERT(
				IsVisible(entity_cmd.entity), "Cannot render entity without visible component"
			);
			impl::InvokeDrawable(ctx, entity_cmd.entity);
			entity_index++;
		} else {
			PTGN_ASSERT(bucket.manual_commands);
			bucket.manual_commands->Draw(renderer_, manual_index);
			manual_index++;
		}
	}

	if (bucket.entity_commands) {
		bucket.entity_commands->clear();
	}

	if (bucket.manual_commands) {
		bucket.manual_commands->Clear();
	}
}

std::vector<impl::CameraRenderBucket> RenderContext::GetRenderBuckets(
	std::vector<impl::CameraRenderCommands>& manual_commands,
	std::vector<impl::CameraEntityCommands>& entity_commands
) {
	PTGN_ASSERT((!ContainsDuplicates(manual_commands, [](const auto& c1, const auto& c2) {
		return c1.camera == c2.camera;
	})));

	PTGN_ASSERT((!ContainsDuplicates(entity_commands, [](const auto& c1, const auto& c2) {
		return c1.camera == c2.camera;
	})));

	std::vector<impl::CameraRenderBucket> buckets;
	buckets.reserve(entity_commands.size() + manual_commands.size());

	auto find_or_create_bucket = [&](const auto& camera) -> impl::CameraRenderBucket& {
		if (auto it{ std::ranges::find_if(
				buckets,
				[&](const impl::CameraRenderBucket& bucket) { return *bucket.camera == camera; }
			) };
			it != buckets.end()) {
			return *it;
		}

		buckets.push_back(
			impl::CameraRenderBucket{
				.camera = &camera,
			}
		);

		return buckets.back();
	};

	for (auto& entity_camera_commands : entity_commands) {
		auto& bucket{ find_or_create_bucket(entity_camera_commands.camera) };
		bucket.entity_commands = &entity_camera_commands.commands;
	}

	for (auto& manual_camera_commands : manual_commands) {
		auto& bucket{ find_or_create_bucket(manual_camera_commands.camera) };
		bucket.manual_commands = &manual_camera_commands.commands;
	}

	std::ranges::stable_sort(buckets, [](const auto& a, const auto& b) {
		if (a.camera->depth < b.camera->depth) {
			return true;
		}

		if (b.camera->depth < a.camera->depth) {
			return false;
		}

		bool a_has_entities{ a.entity_commands && !a.entity_commands->empty() };
		bool b_has_entities{ b.entity_commands && !b.entity_commands->empty() };

		if (a_has_entities != b_has_entities) {
			return a_has_entities;
		}

		return a.camera->uuid < b.camera->uuid;
	});

	return buckets;
}

void RenderContext::SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera) {
	renderer_.SetPrimaryWorldCamera(primary_world_camera);
}

const std::optional<Camera>& RenderContext::GetPrimaryWorldCamera() const {
	return renderer_.GetPrimaryWorldCamera();
}

impl::ShaderId RenderContext::GetShader(std::string_view shader_key) const {
	return renderer_.GetShader(shader_key);
}

} // namespace ptgn