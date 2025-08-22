#include "renderer/vfx/light.h"

#include "common/assert.h"
#include "components/draw.h"
#include "components/effects.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "scene/camera.h"

namespace ptgn {

PointLight::PointLight(const Entity& entity) : Entity{ entity } {}

void PointLight::SetUniform(Entity entity, const Shader& shader) {
	PointLight light{ entity };

	auto transform{ GetDrawTransform(entity) };
	float radius{ light.GetRadius() * Abs(transform.GetAverageScale()) };

	shader.SetUniform("u_LightPosition", transform.GetPosition());
	shader.SetUniform("u_LightIntensity", light.GetIntensity());
	shader.SetUniform("u_LightRadius", radius);
	shader.SetUniform("u_Falloff", light.GetFalloff());
	shader.SetUniform("u_Color", light.GetColor().Normalized());
	auto ambient_color{ PointLight::GetShaderColor(light.GetAmbientColor()) };
	shader.SetUniform("u_AmbientColor", ambient_color);
	shader.SetUniform("u_AmbientIntensity", light.GetAmbientIntensity());
}

void PointLight::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::RenderState state;
	state.blend_mode  = GetBlendMode(entity);
	state.shader_pass = { game.shader.Get<OtherShader::Light>(), &PointLight::SetUniform };
	state.post_fx	  = entity.GetOrDefault<PostFX>();
	state.camera	  = entity.GetOrDefault<Camera>();

	ctx.AddShader(entity, state, color::Transparent, V2_int{}, false);
}

PointLight& PointLight::SetIntensity(float intensity) {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	Get<impl::LightProperties>().intensity = intensity;
	return *this;
}

float PointLight::GetIntensity() const {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	return Get<impl::LightProperties>().intensity;
}

PointLight& PointLight::SetColor(const Color& color) {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	Get<impl::LightProperties>().color = color;
	return *this;
}

Color PointLight::GetColor() const {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	return Get<impl::LightProperties>().color;
}

PointLight& PointLight::SetAmbientIntensity(float ambient_intensity) {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	Get<impl::LightProperties>().ambient_intensity = ambient_intensity;
	return *this;
}

float PointLight::GetAmbientIntensity() const {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	return Get<impl::LightProperties>().ambient_intensity;
}

PointLight& PointLight::SetAmbientColor(const Color& ambient_color) {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	Get<impl::LightProperties>().ambient_color = ambient_color;
	return *this;
}

Color PointLight::GetAmbientColor() const {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	return Get<impl::LightProperties>().ambient_color;
}

PointLight& PointLight::SetRadius(float radius) {
	PTGN_ASSERT(radius > 0.0f, "Point light radius must be above 0");
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	Get<impl::LightProperties>().radius = radius;
	return *this;
}

float PointLight::GetRadius() const {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	return Get<impl::LightProperties>().radius;
}

PointLight& PointLight::SetFalloff(float falloff) {
	PTGN_ASSERT(falloff >= 0.0f, "Point light falloff must be above or equal to 0");
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	Get<impl::LightProperties>().falloff = falloff;
	return *this;
}

float PointLight::GetFalloff() const {
	PTGN_ASSERT(Has<impl::LightProperties>(), "Point light must have LightProperties component");
	return Get<impl::LightProperties>().falloff;
}

V3_float PointLight::GetShaderColor(const Color& color) {
	V4_float n{ color.Normalized() };
	return { n.x, n.y, n.z };
}

PointLight CreatePointLight(
	Manager& manager, const V2_float& position, float radius, const Color& color, float intensity,
	float falloff
) {
	PointLight point_light{ manager.CreateEntity() };

	// Entity properties.

	SetDraw<PointLight>(point_light);
	Show(point_light);
	SetPosition(point_light, position);
	SetBlendMode(point_light, BlendMode::AddPremultipliedWithAlpha);

	// Point light properties.

	auto& light_properties{ point_light.Add<impl::LightProperties>() };

	light_properties.color	   = color;
	light_properties.intensity = intensity;
	light_properties.radius	   = radius;
	light_properties.falloff   = falloff;

	return point_light;
}

} // namespace ptgn