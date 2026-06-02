#include "runtime/graphics/fx/light.h"

#include <array>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/math_utils.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace {

constexpr std::string_view kMaskedLightShader{ "light_masked" };

bool IsInvisibleCone(Entity entity) {
	const auto& light{ entity.Get<impl::LightData>() };
	return light.cone_angle.has_value() && *light.cone_angle == Radians{ 0.0f };
}

void DrawMaskPolygon(DrawContext& ctx, std::span<const V2_float> vertices) {
	if (vertices.size() < 3) {
		return;
	}

	Polygon polygon{ vertices };

	ctx.DrawShape(
		{}, polygon, color::White,
		ShapeDrawParams{
			.depth		= 0.0f,
			.fill_style = Solid{},
			.origin		= Origin::Center,
		}
	);
}

void RenderVisibilityMask(
	DrawContext& ctx, impl::RenderTargetObject& mask_target,
	const impl::VisibilityPolygon& visibility_polygon, V2_int target_size
) {
	auto mask_id{ mask_target.operator impl::RenderTargetId() };
	Viewport viewport{ .position{}, .size{ target_size } };

	ctx.WithRenderTarget(mask_target, viewport, [&]() {
		ctx.ClearRenderTarget(mask_id, color::Black, false, false);

		ctx.WithBlendMode(BlendMode::ReplaceRGBA, [&]() {
			DrawMaskPolygon(ctx, visibility_polygon.vertices);

			for (const auto& interior : visibility_polygon.occluder_interiors) {
				if (interior.masks_light_inside) {
					continue;
				}

				DrawMaskPolygon(ctx, interior.vertices);
			}
		});
	});
}

void RenderMaskedLight(
	DrawContext& ctx, Entity entity, impl::RenderTargetObject& light_target,
	const impl::RenderTargetObject& mask_target, V2_int target_size,
	std::vector<UniformWrite> uniforms
) {
	auto mask_id{ mask_target.operator impl::RenderTargetId() };
	auto light_id{ light_target.operator impl::RenderTargetId() };

	auto mask_input{ impl::BoundInput{
		.render_target = mask_id,
		.binding	   = TextureBinding{ .slot = 0, .uniform = "u_ShadowMask" },
	} };

	Viewport viewport{ .position{}, .size{ target_size } };

	ctx.WithRenderTarget(light_target, viewport, [&]() {
		ctx.DrawRenderPass(
			impl::DrawPassRequest{
				.material =
					MaterialState{
						.shader	  = ctx.GetShader(kMaskedLightShader),
						.uniforms = std::move(uniforms),
					},
				.pipeline = Hash("texture"),
				.inputs	  = std::span{ &mask_input, 1 },
				.output	  = light_id,
				.viewport = viewport,
				.tint	  = color::White,
			}
		);
	});
}

void DrawUnmaskedLight(
	DrawContext& ctx, Entity entity, Transform draw_transform, V2_float size, BlendMode blend_mode,
	std::vector<UniformWrite> uniforms
) {
	Material material{
		.shader	  = "light",
		.uniforms = std::move(uniforms),
	};

	auto params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	ctx.WithBlendMode(blend_mode, [&]() {
		ctx.DrawShader(draw_transform, material, std::move(params));
	});
}

void DrawShadowedLight(
	DrawContext& ctx, Entity entity, Transform draw_transform, V2_float size, BlendMode blend_mode,
	const impl::VisibilityPolygon& visibility_polygon, std::vector<UniformWrite> uniforms
) {
	auto target_size{ V2_int{ size } };

	PTGN_ASSERT(target_size.IsPositive(), "Light shadow target size must be positive");

	RenderTargetDesc desc{
		.size	= target_size,
		.format = TextureFormat::RGBA8,
	};

	auto mask_target{ ctx.CreateTemporaryRenderTarget(desc) };
	auto light_target{ ctx.CreateTemporaryRenderTarget(desc) };

	RenderVisibilityMask(ctx, mask_target, visibility_polygon, target_size);
	RenderMaskedLight(ctx, entity, light_target, mask_target, target_size, std::move(uniforms));

	auto light_id{ light_target.operator impl::RenderTargetId() };
	auto light_texture{ ctx.GetRenderTargetTexture(light_id) };

	auto params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	ctx.WithBlendMode(blend_mode, [&]() {
		ctx.DrawTexture(draw_transform, light_texture, std::move(params));
	});

	ctx.PreserveTemporaryRenderTarget(std::move(light_target));
	ctx.PreserveTemporaryRenderTarget(std::move(mask_target));
}

} // namespace

Light::Light(Entity entity) : Entity{ entity } {}

std::array<UniformWrite, 9> Light::GetUniforms() const {
	const auto& light{ Get<impl::LightData>() };

	auto color{ GetTint(*this) };
	V4_float color_n{ color.Normalized() };

	auto ambient_light_n{ light.ambient_color.Normalized() };
	V3_float ambient_color{ ambient_light_n.xyz() };
	constexpr V3_float light_attenuation{ 1.0f, 0.0f, 0.1f };

	return { { { "u_LightIntensity", light.intensity },
			   { "u_LightRadius", 0.5f },
			   { "u_Falloff", light.falloff },
			   { "u_UseCone", light.cone_angle.has_value() ? 1.0f : 0.0f },
			   { "u_ConeAngle",
				 light.cone_angle.has_value() ? (*light.cone_angle / 2.0f).value : kTwoPi },
			   { "u_Color", color_n },
			   { "u_AmbientColor", ambient_color },
			   { "u_AmbientIntensity", light.ambient_intensity },
			   { "u_LightAttenuation", light_attenuation } } };
}

void Light::Draw(DrawContext& ctx, Entity entity) {
	PTGN_ASSERT((entity.Has<Circle, impl::LightData>()));

	if (IsInvisibleCone(entity)) {
		return;
	}

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& circle{ entity.Get<Circle>() };
	auto size{ circle.GetSize() };
	auto blend_mode{ GetBlendMode(entity) };

	auto uniforms{ std::ranges::to<std::vector<UniformWrite>>(Light{ entity }.GetUniforms()) };

	if (!entity.Has<impl::VisibilityPolygon>()) {
		DrawUnmaskedLight(ctx, entity, draw_transform, size, blend_mode, std::move(uniforms));
		return;
	}

	const auto& visibility_polygon{ entity.Get<impl::VisibilityPolygon>() };

	if (visibility_polygon.vertices.empty()) {
		return;
	}

	DrawShadowedLight(
		ctx, entity, draw_transform, size, blend_mode, visibility_polygon, std::move(uniforms)
	);
}

Light& Light::SetIntensity(float intensity) {
	Get<impl::LightData>().intensity = intensity;
	return *this;
}

float Light::GetIntensity() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().intensity;
}

Light& Light::SetColor(Color color) {
	Add<impl::Tint>(color);
	return *this;
}

Color Light::GetColor() const {
	PTGN_ASSERT(Has<impl::Tint>(), "Light must have Tint component");
	return Get<impl::Tint>();
}

Light& Light::SetAmbientIntensity(float ambient_intensity) {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	Get<impl::LightData>().ambient_intensity = ambient_intensity;
	return *this;
}

float Light::GetAmbientIntensity() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().ambient_intensity;
}

Light& Light::SetAmbientColor(Color ambient_color) {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	Get<impl::LightData>().ambient_color = ambient_color;
	return *this;
}

Color Light::GetAmbientColor() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().ambient_color;
}

Light& Light::SetRadius(float radius) {
	PTGN_ASSERT(radius > 0.0f, "Light radius must be above 0");
	Add<Circle>().radius = radius;
	return *this;
}

float Light::GetRadius() const {
	PTGN_ASSERT(Has<Circle>(), "Light must have Circle component");
	return Get<Circle>().radius;
}

Light& Light::SetFalloff(float falloff) {
	PTGN_ASSERT(falloff >= 0.0f, "Light falloff must be above or equal to 0");
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	Get<impl::LightData>().falloff = falloff;
	return *this;
}

float Light::GetFalloff() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().falloff;
};

Light& Light::SetConeAngle(std::optional<Degrees> cone_angle) {
	PTGN_ASSERT(Has<impl::LightData>(), "Directional light must have LightData component");
	auto& light_data{ Get<impl::LightData>() };

	if (!cone_angle.has_value()) {
		light_data.cone_angle = std::nullopt;
		return *this;
	}

	light_data.cone_angle = Radians{ Clamp(*cone_angle) };

	return *this;
}

std::optional<Degrees> Light::GetConeAngle() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	if (const auto& light_data{ Get<impl::LightData>() }; light_data.cone_angle.has_value()) {
		return light_data.cone_angle->ToDeg();
	} else {
		return std::nullopt;
	}
}

Light& Light::SetLightProperties(const LightProperties& properties) {
	SetRadius(properties.radius);
	SetColor(properties.color);
	SetIntensity(properties.intensity);
	SetFalloff(properties.falloff);
	SetConeAngle(properties.cone_angle);
	SetRotation(*this, properties.direction_angle);
	return *this;
}

LightProperties Light::GetLightProperties() const {
	LightProperties properties;
	properties.radius	  = GetRadius();
	properties.color	  = GetColor();
	properties.intensity  = GetIntensity();
	properties.falloff	  = GetFalloff();
	properties.cone_angle = GetConeAngle();
	auto rotation{ GetRotation(*this) };
	properties.direction_angle = rotation;
	return properties;
}

Light CreateLight(Scene& scene, V2_float position, const LightProperties& properties) {
	Light light{ scene.CreateEntity() };
	light.Add<impl::LightData>();
	light.SetLightProperties(properties);

	SetPosition(light, position);
	SetDraw<Light>(light);
	Show(light);

	// Blend mode with which the lights are added to the scene.
	SetBlendMode(light, BlendMode::PremultipliedAddRGBA);

	return light;
}

} // namespace ptgn