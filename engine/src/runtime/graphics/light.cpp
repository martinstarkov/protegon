#include "runtime/graphics/light.h"

#include <algorithm>
#include <array>
#include <optional>

#include "core/assert.h"
#include "core/math/geometry/circle.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Light::Light(Entity entity) : Entity{ entity } {}

void Light::SetUniform(DrawContext& renderer, Entity entity) {
	const auto& light{ entity.Get<impl::LightData>() };

	auto color{ GetTint(entity) };
	V4_float color_n{ color.Normalized() };

	auto ambient_light_n{ light.ambient_color.Normalized() };
	V3_float ambient_color{ ambient_light_n.xyz() };
	constexpr V3_float light_attenuation{ 1.0f, 0.0f, 0.1f };

	auto light_shader{ renderer.GetShader("light") };

	renderer.SetUniform(light_shader, "u_LightIntensity", light.intensity);
	renderer.SetUniform(light_shader, "u_LightRadius", 0.5f);
	renderer.SetUniform(light_shader, "u_Falloff", light.falloff);

	if (light.cone_angle.has_value()) {
		renderer.SetUniform(light_shader, "u_UseCone", 1.0f);
		renderer.SetUniform(light_shader, "u_ConeAngle", *light.cone_angle / 2.0f);
	} else {
		renderer.SetUniform(light_shader, "u_UseCone", 0.0f);
		renderer.SetUniform(light_shader, "u_ConeAngle", DegToRad(360.0f));
	}

	renderer.SetUniform(light_shader, "u_Color", color_n);
	renderer.SetUniform(light_shader, "u_AmbientColor", ambient_color);
	renderer.SetUniform(light_shader, "u_AmbientIntensity", light.ambient_intensity);
	renderer.SetUniform(light_shader, "u_LightAttenuation", light_attenuation);
}

void Light::Draw(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT((entity.Has<Circle, impl::LightData>()));

	if (const auto& light{ entity.Get<impl::LightData>() };
		light.cone_angle.has_value() && *light.cone_angle == 0.0f) {
		// Directional light with cone angle of 0.0f.
		return;
	}

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& circle{ entity.Get<Circle>() };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto positions{ circle.GetWorldQuadVertices(draw_transform) };
	auto blend_mode{ GetBlendMode(entity) };

	auto light_shader{ renderer.GetShader("light") };

	std::array<float, 4> user_data{};

	auto shader_setup = [&renderer, entity]() {
		SetUniform(renderer, entity);
	};

	constexpr bool floor_positions{ true };

	renderer.SetBlend(blend_mode);
	renderer.DrawQuad(
		light_shader, positions, user_data, tint, depth, shader_setup, floor_positions
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

Light& Light::SetConeAngle(std::optional<float> cone_angle) {
	PTGN_ASSERT(Has<impl::LightData>(), "Directional light must have LightData component");
	auto& light_data{ Get<impl::LightData>() };

	if (!cone_angle.has_value()) {
		light_data.cone_angle = std::nullopt;
		return *this;
	}

	float angle = std::clamp(*cone_angle, 0.0f, 360.0f);

	light_data.cone_angle = DegToRad(angle);

	return *this;
}

std::optional<float> Light::GetConeAngle() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	if (const auto& light_data{ Get<impl::LightData>() }; light_data.cone_angle.has_value()) {
		return RadToDeg(*light_data.cone_angle);
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
	SetRotation(*this, DegToRad(properties.direction_angle));
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
	properties.direction_angle = RadToDeg(rotation);
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