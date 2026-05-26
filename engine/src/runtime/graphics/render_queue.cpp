#include "runtime/graphics/render_queue.h"

#include <algorithm>
#include <array>
#include <compare>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_command.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/shape_primitives.h"
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

impl::CommonShapeParams ConvertToCommonShapeParams(
	Transform transform, Color color, const ShapeRenderParams& params
) {
	return { .transform{ transform },
			 .fill_style{ params.fill_style },
			 .draw_origin{ params.origin },
			 .color{ color },
			 .depth{ params.depth },
			 .entity_id{ params.entity_id } };
}

template <ShapeType TShape>
void DrawShapeImpl(
	impl::RenderCommands& commands, impl::ShaderId shader, Transform transform, const TShape& shape,
	Color color, const ShapeRenderParams& params
) {
	impl::VisitPrimitives(
		shape, ConvertToCommonShapeParams(transform, color, params), [&](auto& primitives) {
			commands.Add(shader, primitives, params.blend_mode, params.depth, {});
		}
	);
}

} // namespace

RenderQueue::RenderQueue(Scene& scene, Renderer& renderer) :
	scene_{ scene }, renderer_{ renderer } {}

impl::RenderCommands& RenderQueue::GetRenderCommands(
	const std::optional<impl::RenderCamera>& camera, bool debug
) {
	impl::RenderCamera cam;

	if (camera.has_value()) {
		cam = *camera;
	} else {
		cam = impl::RenderCamera{ scene_.ctx().camera };
	}

	std::vector<impl::CameraRenderCommands>* commands{ nullptr };

	if (debug) {
		commands = &debug_commands_;
	} else {
		commands = &render_commands_;
	}

	PTGN_ASSERT(commands != nullptr);

	for (auto& [c, cmds] : *commands) {
		if (c == cam) {
			return cmds;
		}
	}
	return commands->emplace_back(cam, impl::RenderCommands{}).commands;
}

void RenderQueue::DrawTexture(
	Transform transform, impl::TextureId texture, V2_int texture_size, impl::ShaderId shader,
	TextureRenderParams params
) {
	Rect rect{ params.size.value_or(V2_float{ texture_size }) };

	auto positions{ rect.GetWorldVertices(transform, params.origin) };

	auto tex_coords{ params.texture_coordinates.value_or(
		// impl::GetDefaultTextureCoordinates<false>()
		impl::GetTextureCoordinates({}, texture_size, texture_size, false, true)
	) };

	auto quad{ impl::CreateTextureQuad(
		positions, params.depth, params.tint.Normalized(), tex_coords, params.entity_id
	) };

	std::span primitives{ &quad, 1 };

	auto& commands{ GetRenderCommands(params.camera, false) };

	commands.Add(shader, primitives, params.blend_mode, params.depth, texture);
}

void RenderQueue::DrawTexture(
	Transform transform, std::string_view texture_key, TextureRenderParams params
) {
	auto shader{ GetShader("texture") };
	const auto& assets{ scene_.ctx().asset };
	auto texture{ assets.Get<Texture>(texture_key) };
	auto texture_size{ texture.GetSize() };

	DrawTexture(transform, texture, texture_size, shader, std::move(params));
}

void RenderQueue::DrawTexture(
	Transform transform, std::string_view texture_key, std::string_view shader_key,
	TextureRenderParams params
) {
	const auto& assets{ scene_.ctx().asset };
	auto texture{ assets.Get<Texture>(texture_key) };
	auto shader{ assets.Get<Shader>(shader_key) };
	auto texture_size{ texture.GetSize() };

	DrawTexture(transform, texture, texture_size, shader, std::move(params));
}

void RenderQueue::DrawShader(
	Transform transform, std::string_view shader_key, TextureRenderParams params
) {
	const auto& assets{ scene_.ctx().asset };
	auto shader{ assets.Get<Shader>(shader_key) };
	auto texture_size{ renderer_.GetGameSize() };
	impl::TextureId texture{};

	DrawTexture(transform, texture, texture_size, shader, std::move(params));
}

void RenderQueue::DrawPoint(V2_float point, Color color, ShapeRenderParams params) {
	DrawShape({}, point, color, std::move(params));
}

void RenderQueue::DrawLine(V2_float start, V2_float end, Color color, ShapeRenderParams params) {
	DrawShape({}, Line{ start, end }, color, std::move(params));
}

void RenderQueue::DrawLines(
	std::span<const V2_float> points, Color color, ShapeRenderParams params, bool closed,
	std::optional<Transform> transform
) {
	auto primitives{ impl::GetHollowPrimitives(
		points, closed, ConvertToCommonShapeParams(transform.value_or(Transform{}), color, params)
	) };

	auto& commands{ GetRenderCommands(params.camera, params.debug) };

	commands.Add(GetShader("color"), primitives, params.blend_mode, params.depth, {});
}

void RenderQueue::DrawShape(
	Transform transform, const V2_float& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("color"), transform, shape, color,
		params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Rect& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("color"), transform, shape, color,
		params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const RoundedRect& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("rounded_rect"), transform, shape,
		color, params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Polygon& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("color"), transform, shape, color,
		params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Triangle& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("color"), transform, shape, color,
		params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Capsule& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("capsule"), transform, shape,
		color, params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Line& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("color"), transform, shape, color,
		params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Arc& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("arc"), transform, shape, color,
		params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Circle& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("ellipse"), transform, shape,
		color, params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Ellipse& shape, Color color, ShapeRenderParams params
) {
	DrawShapeImpl(
		GetRenderCommands(params.camera, params.debug), GetShader("ellipse"), transform, shape,
		color, params
	);
}

void RenderQueue::DrawShape(
	Transform transform, const Shape& shape, Color color, ShapeRenderParams params
) {
	shape.Visit([&](const auto& specific_shape) {
		DrawShape(transform, specific_shape, color, std::move(params));
	});
}

impl::ShaderId RenderQueue::GetShader(std::string_view shader_key) const {
	return impl::RendererAccessor{ renderer_ }.GetShader(shader_key);
}

// TODO: Fix.
// void RenderQueue::DrawText(
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
//		texture_id, texture_size, texture_shader, transform, text_size, draw_origin,
// color::White, 		depth, blend_mode, {}, camera, entity_id
//	);
// }

void RenderQueue::CombineCommands(const impl::RenderCamera& camera) {
	impl::CameraRenderCommands combined_debug{ .camera = camera };
	impl::CameraRenderCommands combined_render{ .camera = camera };

	auto combine = [](auto& commands, auto& combined) {
		for (auto& bucket : commands) {
			combined.commands.CombineWith(std::move(bucket.commands));
		}

		commands.clear();
		commands.emplace_back(std::move(combined));
	};

	combine(debug_commands_, combined_debug);
	combine(render_commands_, combined_render);
}

void RenderQueue::SetupCamera(
	const RenderTarget& scene_render_target, impl::ClearedEntities& cleared, V2_int game_size,
	const impl::RenderCamera& render_camera
) {
	auto render_target{ render_camera.render_target ? render_camera.render_target
													: scene_render_target };

	impl::RendererAccessor renderer{ renderer_ };

	renderer.SetRenderTarget(&render_target.Get<impl::RenderTargetObject>());

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
	renderer.SetViewport(viewport);
	renderer.SetViewProjection(render_camera.camera.view_projection);

	if (bool clear_camera{ !std::ranges::contains(cleared.cameras, render_camera.uuid) };
		clear_camera && render_camera.clear_color.has_value()) {
		renderer.SetScissor(ScissorState{ viewport });
		render_target.Clear(*render_camera.clear_color, false);
		renderer.SetScissor(ScissorState{ false });
		cleared.cameras.emplace_back(render_camera.uuid);
	}
}

void RenderQueue::Draw(
	DrawContext& ctx, const RenderTarget& scene_render_target, impl::ClearedEntities& cleared,
	V2_int game_size, const std::vector<impl::CameraRenderBucket>& buckets
) {
	for (const auto& bucket : buckets) {
		Draw(ctx, scene_render_target, cleared, game_size, bucket);
	}
}

void RenderQueue::Draw(
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

std::vector<impl::CameraRenderBucket> RenderQueue::GetRenderBuckets(
	std::vector<impl::CameraRenderCommands>& manual_commands,
	std::vector<impl::CameraEntityCommands>& entity_commands
) {
	PTGN_ASSERT((!ContainsDuplicates(manual_commands, &impl::CameraRenderCommands::camera)));
	PTGN_ASSERT((!ContainsDuplicates(entity_commands, &impl::CameraEntityCommands::camera)));

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

} // namespace ptgn