#include "runtime/graphics/fx/light.h"

#include <array>
#include <optional>
#include <ranges>
#include <span>
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
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/framebuffer.h"
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

constexpr int kLightVisibleStencilRef{ 1 };

bool IsInvisibleCone(Entity entity) {
	const auto& light{ entity.Get<impl::LightData>() };
	return light.cone_angle.has_value() && *light.cone_angle == Radians{ 0.0f };
}

void DrawStencilPolygon(
	DrawContext& ctx, Transform draw_transform, std::span<const V2_float> vertices
) {
	if (vertices.size() < 3) {
		return;
	}

	Polygon polygon{ vertices };

	ctx.DrawShape(
		draw_transform.Inverse(), polygon, color::White,
		ShapeDrawParams{
			.depth		= 0.0f,
			.fill_style = Solid{},
			.origin		= Origin::Center,
		}
	);
}

RenderStateDelta ReplaceStencilState(int value) {
	return RenderStateDelta{
		.blending	   = false,
		.depth_testing = false,
		.color_mask =
			ColorMaskState{
				.red   = false,
				.green = false,
				.blue  = false,
				.alpha = false,
			},
		.stencil =
			StencilState{
				.enabled	= true,
				.func		= CompareFunc::Always,
				.ref		= value,
				.mask		= 0xFF,
				.fail_op	= StencilOp::Keep,
				.zfail_op	= StencilOp::Keep,
				.zpass_op	= StencilOp::Replace,
				.write_mask = 0xFF,
			},
	};
}

RenderStateDelta ReadStencilState(int value) {
	return RenderStateDelta{
		.blending	   = false,
		.depth_testing = false,
		.color_mask =
			ColorMaskState{
				.red   = true,
				.green = true,
				.blue  = true,
				.alpha = true,
			},
		.stencil =
			StencilState{
				.enabled	= true,
				.func		= CompareFunc::Equal,
				.ref		= value,
				.mask		= 0xFF,
				.fail_op	= StencilOp::Keep,
				.zfail_op	= StencilOp::Keep,
				.zpass_op	= StencilOp::Keep,
				.write_mask = 0x00,
			},
	};
}

void WriteLightVisibilityStencil(
	DrawContext& ctx, Transform draw_transform, const impl::VisibilityPolygon& visibility_polygon
) {
	ctx.WithRenderState(
		ReplaceStencilState(kLightVisibleStencilRef),
		[&ctx, &visibility_polygon, draw_transform]() {
			DrawStencilPolygon(ctx, draw_transform, visibility_polygon.vertices);

			for (const auto& interior : visibility_polygon.occluder_interiors) {
				if (interior.masks_light_inside) {
					continue;
				}

				DrawStencilPolygon(ctx, draw_transform, interior.vertices);
			}
		}
	);

	// Optional, but useful if the visibility polygon includes blocker interiors.
	// These interiors are forced back to stencil 0, so the light will not draw there.
	ctx.WithRenderState(ReplaceStencilState(0), [&ctx, &visibility_polygon, draw_transform]() {
		for (const auto& interior : visibility_polygon.occluder_interiors) {
			if (!interior.masks_light_inside) {
				continue;
			}

			DrawStencilPolygon(ctx, draw_transform, interior.vertices);
		}
	});
}

void DrawLightThroughStencil(
	DrawContext& ctx, Entity entity, V2_float size, std::vector<UniformWrite> uniforms
) {
	Material material{
		.shader	  = "light",
		.uniforms = std::move(uniforms),
	};

	auto params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	// This draw happens into the local light target, not into the world scene.
	params.depth   = 0.0f;
	params.origin  = Origin::Center;
	params.effects = {};

	ctx.WithRenderState(ReadStencilState(kLightVisibleStencilRef), [&ctx, &material, &params]() {
		ctx.DrawShader({}, material, std::move(params));
	});
}

void DrawShadowedLight(
	DrawContext& ctx, Entity entity, Transform draw_transform, V2_float size, BlendMode blend_mode,
	const impl::VisibilityPolygon& visibility_polygon, std::vector<UniformWrite> uniforms
) {
	V2_int target_size{ size };

	PTGN_ASSERT(target_size.IsPositive(), "Light shadow target size must be positive");

	TextureDesc light_desc{
		.size	= target_size,
		.format = TextureFormat::RGBA8,
	};

	TextureDesc shadow_desc{ .size = target_size, .format = TextureFormat::Stencil8 };

	auto composite_params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	Viewport viewport{
		.position{},
		.size{ target_size },
	};

	ctx.WithTemporaryFramebuffer(
		light_desc, shadow_desc,
		[&ctx, viewport, &visibility_polygon, entity, size, &uniforms, blend_mode,
		 draw_transform](impl::FramebufferObject& framebuffer) {
			framebuffer.Clear(color::Transparent, true);
			framebuffer.Clear(Stencil{ 0 }, true);

			ctx.WithRenderTarget(
				&framebuffer, viewport,
				[&ctx, &visibility_polygon, entity, size, &uniforms, draw_transform]() {
					WriteLightVisibilityStencil(ctx, draw_transform, visibility_polygon);
					DrawLightThroughStencil(ctx, entity, size, std::move(uniforms));
				}
			);

			auto texture{ framebuffer.GetTexture() };
			auto composite_params{ impl::GetTextureDrawParams(entity, size, true, color::White) };

			ctx.WithBlendMode(blend_mode, [&ctx, draw_transform, texture, &composite_params]() {
				ctx.DrawTexture(draw_transform, texture, std::move(composite_params));
			});
		}
	);
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

	ctx.WithBlendMode(blend_mode, [&ctx, draw_transform, &material, &params]() {
		ctx.DrawShader(draw_transform, material, std::move(params));
	});
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