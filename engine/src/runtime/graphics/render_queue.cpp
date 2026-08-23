#include "runtime/graphics/render_queue.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
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
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_command.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/shape_primitives.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

impl::CommonShapeParams ConvertToCommonShapeParams(
	Transform transform, Color color, const ShapeRenderParams& params
) {
	return { .transform{ transform },
			 .fill_style{ params.fill_style },
			 .origin = params.origin,
			 .color{ color.Normalized() },
			 .depth{ params.depth },
			 .entity_id = params.entity_id };
}

template <ShapeType TShape>
void DrawShapeImpl(
	impl::RenderCommands& commands, impl::ShaderId shader, Transform transform, const TShape& shape,
	Color color, const ShapeRenderParams& params
) {
	impl::VisitPrimitives(
		shape, ConvertToCommonShapeParams(transform, color, params),
		[&commands, shader, &params](auto& primitives) {
			commands.Add({ .shader = shader }, primitives, params.blend_mode, params.depth, {});
		}
	);
}

} // namespace

RenderQueue::RenderQueue(Renderer& renderer, Scene& scene) :
	scene_{ &scene }, renderer_{ renderer } {}

impl::RenderCommands& RenderQueue::GetRenderCommands(
	std::optional<SceneCamera> camera, bool debug
) {
	PTGN_ASSERT(scene_);

	camera =
		camera.or_else([this]() -> std::optional<SceneCamera> { return scene_->ctx().camera; });

	PTGN_ASSERT(camera.has_value(), "Invalid camera");

	auto& commands{ debug ? debug_commands_ : render_commands_ };

	for (auto& camera_render : commands) {
		if (camera_render.camera == camera.value()) {
			return camera_render.commands;
		}
	}

	return commands.emplace_back(impl::CameraRenderCommands{
		.camera = camera.value(),
		.commands = {},
	}).commands;
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

	commands.Add({ .shader = shader }, primitives, params.blend_mode, params.depth, texture);
}

void RenderQueue::DrawTexture(
	Transform transform, TextureKey texture_key, TextureRenderParams params
) {
	PTGN_ASSERT(scene_);

	auto shader{ GetShader("texture") };
	auto& assets{ scene_->ctx().asset };

	if (!impl::AssetAccessor{ assets }.Has<Texture>(texture_key)) {
		PTGN_WARN(
			"Cannot draw texture with key that is not loaded in the asset manager: ", texture_key
		);
		return;
	}

	auto texture{ impl::AssetAccessor{ assets }.Get<Texture>(texture_key) };
	auto texture_size{ texture.GetSize() };

	DrawTexture(transform, texture, texture_size, shader, std::move(params));
}

void RenderQueue::DrawTexture(
	Transform transform, TextureKey texture_key, ShaderKey shader_key,
	TextureRenderParams params
) {
	PTGN_ASSERT(scene_);

	auto& assets{ scene_->ctx().asset };

	if (!impl::AssetAccessor{ assets }.Has<Texture>(texture_key)) {
		PTGN_WARN(
			"Cannot draw texture with key that is not loaded in the asset manager: ", texture_key
		);
		return;
	}

	if (!impl::AssetAccessor{ assets }.Has<Shader>(shader_key)) {
		PTGN_WARN(
			"Cannot draw texture with shader key that is not loaded in the asset manager: ",
			shader_key
		);
		return;
	}

	auto texture{ impl::AssetAccessor{ assets }.Get<Texture>(texture_key) };
	auto shader{ impl::AssetAccessor{ assets }.Get<Shader>(shader_key) };

	auto texture_size{ texture.GetSize() };

	DrawTexture(transform, texture, texture_size, shader, std::move(params));
}

void RenderQueue::DrawShader(
	Transform transform, ShaderKey shader_key, TextureRenderParams params
) {
	PTGN_ASSERT(scene_);
	auto& assets{ scene_->ctx().asset };

	if (!impl::AssetAccessor{ assets }.Has<Shader>(shader_key)) {
		PTGN_WARN(
			"Cannot draw texture with shader key that is not loaded in the asset manager: ",
			shader_key
		);
		return;
	}

	auto shader{ impl::AssetAccessor{ assets }.Get<Shader>(shader_key) };
	auto texture_size{ renderer_.GetLogicalSize() };

	DrawTexture(transform, {}, texture_size, shader, std::move(params));
}

void RenderQueue::DrawText(
	Transform transform, std::string_view text, Color color, float font_size,
	const TextBox& text_box, TextRenderParams params
) {
	DrawText(
		transform,
		StyledText{ TextRun{ .text	= std::string{ text },
							 .style = { .color = color, .size = font_size } } },
		text_box, std::move(params)
	);
}

void RenderQueue::DrawText(
	Transform transform, StyledText styled_text, TextBox text_box, TextRenderParams params
) {
	PTGN_ASSERT(scene_);

	auto& ctx{ scene_->ctx() };

	auto layout{ impl::BuildTextLayout(ctx.asset, styled_text, text_box) };

	auto prepared{ impl::PrepareTextDraw(transform, layout, text_box, params.origin) };

	if (!prepared.drawable) {
		return;
	}

	auto text_batches{ impl::BuildTextDrawBatches(
		DrawTextRequest{
			.layout	   = layout,
			.tint	   = params.tint,
			.depth	   = params.depth,
			.entity_id = params.entity_id,
			.clips	   = prepared.GetClips(),
			.time	   = ctx.TimeSinceStartSeconds().count(),
		}
	) };

	if (text_batches.empty()) {
		return;
	}

	auto& commands{ GetRenderCommands(params.camera, params.debug) };

	for (auto& batch : text_batches) {
		if (batch.quads.empty()) {
			continue;
		}

		impl::ApplyTransform(prepared.transform, std::span{ batch.quads });

		MaterialState material{
			.shader	  = GetShader("text"),
			.uniforms = impl::GetTextUniforms(batch.style.sdf, batch.decoration),
		};

		commands.Add(material, batch.quads, params.blend_mode, params.depth, batch.style.texture);
	}
}

void RenderQueue::DrawPoint(V2_float point, Color color, ShapeRenderParams params) {
	DrawShape({}, point, color, std::move(params));
}

void RenderQueue::DrawLine(
	V2_float start, V2_float end, Color color, ShapeRenderParams params, Transform transform
) {
	DrawShape(transform, Line{ start, end }, color, std::move(params));
}

void RenderQueue::DrawLines(
	std::span<const V2_float> points, Color color, ShapeRenderParams params, bool closed,
	Transform transform
) {
	auto primitives{ impl::GetHollowPrimitives(
		points, closed, ConvertToCommonShapeParams(transform, color, params)
	) };

	auto& commands{ GetRenderCommands(params.camera, params.debug) };

	commands.Add({ .shader = GetShader("color") }, primitives, params.blend_mode, params.depth, {});
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
	shape.Visit([this, transform, color, &params](const auto& specific_shape) {
		DrawShape(transform, specific_shape, color, std::move(params));
	});
}

impl::ShaderId RenderQueue::GetShader(ShaderKey shader_key) const {
	return impl::RendererAccessor{ renderer_ }.GetShader(shader_key);
}

void RenderQueue::CombineCommands() {
	impl::CameraRenderCommands combined_debug;
	impl::CameraRenderCommands combined_render;

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

void RenderQueue::Rebind(Scene& parent_scene) {
	scene_ = &parent_scene;
}

} // namespace ptgn