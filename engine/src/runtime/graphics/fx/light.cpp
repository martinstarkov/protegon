#include "runtime/graphics/fx/light.h"

#include <array>
#include <optional>
#include <ranges>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

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

	if (const auto& light{ entity.Get<impl::LightData>() };
		light.cone_angle.has_value() && *light.cone_angle == Radians{ 0.0f }) {
		// Directional light with cone angle of 0.0f.
		return;
	}

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& circle{ entity.Get<Circle>() };
	auto size{ circle.GetSize() };
	constexpr auto draw_origin{ Origin::Center };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };
	auto entity_id{ entity.GetUUID() };
	auto effects{ impl::GetEffectParams(entity) };

	constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };

	MaterialState material;

	material.shader	  = ctx.GetShader("light");
	material.uniforms = std::ranges::to<std::vector<UniformWrite>>(Light{ entity }.GetUniforms());

	ctx.WithBlendMode(blend_mode, [&]() {
		ctx.DrawShader(
			material, draw_transform, depth, size, draw_origin, tint, tex_coords, effects, entity_id
		);
	});
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
	Add<Circle>().SetRadius(radius);
	return *this;
}

float Light::GetRadius() const {
	PTGN_ASSERT(Has<Circle>(), "Light must have Circle component");
	return Get<Circle>().GetRadius();
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