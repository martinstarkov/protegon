#include "rendering/graphics/vfx/light.h"

#include <map>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/batching/batch.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/shader.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

PointLight CreatePointLight(
	Scene& scene, const V2_float& position, float radius, const Color& color, float intensity,
	float falloff
) {
	PointLight point_light{ scene.CreateEntity() };

	// Entity properties.

	point_light.SetDraw<PointLight>();
	point_light.Show();
	point_light.SetPosition(position);

	// Point light properties.

	auto& light_properties{ point_light.Add<impl::LightProperties>() };

	light_properties.color	   = color;
	light_properties.intensity = intensity;
	light_properties.radius	   = radius;
	light_properties.falloff   = falloff;

	return point_light;
}

PointLight::PointLight(const Entity& entity) : Entity{ entity } {}

void PointLight::SetUniform(const Entity& entity, const Shader& shader) {
	PointLight light{ entity };

	auto offset_transform{ GetOffset(entity) };
	auto transform{ entity.GetAbsoluteTransform() };
	transform = transform.RelativeTo(offset_transform);
	float radius{ light.GetRadius() * Abs(transform.scale.x) };

	shader.SetUniform("u_LightPosition", transform.position);
	shader.SetUniform("u_LightIntensity", light.GetIntensity());
	shader.SetUniform("u_LightRadius", radius);
	shader.SetUniform("u_Falloff", light.GetFalloff());
	shader.SetUniform("u_Color", light.GetColor().Normalized());
	auto ambient_color{ PointLight::GetShaderColor(light.GetAmbientColor()) };
	shader.SetUniform("u_AmbientColor", ambient_color);
	shader.SetUniform("u_AmbientIntensity", light.GetAmbientIntensity());
}

void PointLight::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::RenderState render_state;
	render_state.blend_mode	   = BlendMode::Add;
	render_state.shader_passes = { impl::ShaderPass{ game.shader.Get<OtherShader::Light>(),
													 &SetUniform } };
	ctx.AddShader(entity, render_state, BlendMode::Add, color::Black, false);
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

/*
void LightManager::Draw() const {
   if (IsEmpty()) {
	   return;
   }
   // TODO: Put these in light.Draw function.
   auto old_target{ game.renderer.GetRenderTarget() };
   auto old_blend_mode{ game.renderer.GetBlendMode() };
   game.renderer.SetBlendMode(BlendMode::AddPremultiplied);

   // Draw each light to the target.
   ForEachValue([&](const Light& light) {
	   // TODO: Make a light.DrawImpl function and put this there.
	   game.renderer.SetRenderTarget(target_);
	   light_shader_.Bind();
	   light_shader_.SetUniform("u_LightPosition", light.GetPosition());
	   light_shader_.SetUniform("u_LightIntensity", light.GetIntensity());
	   light_shader_.SetUniform("u_LightAttenuation", light.attenuation_);
	   light_shader_.SetUniform("u_LightRadius", light.radius_);
	   light_shader_.SetUniform("u_Falloff", light.compression_);
	   V4_float n{ light.ambient_color_.Normalized() };
	   V3_float c{ n.x, n.y, n.z };
	   light_shader_.SetUniform("u_AmbientColor", c);
	   light_shader_.SetUniform("u_AmbientIntensity", light.ambient_intensity_);
	   target_.Draw(TextureInfo{ light.GetColor() }, light_shader_, false);

	   // Draw lights to the bound target.
	   game.renderer.SetRenderTarget(old_target);
	   // TODO: Add optional blurring with blur shader.
	   target_.Draw();
   });

   // TODO: Put this in light.Draw function.
   game.renderer.SetBlendMode(old_blend_mode);
}
*/

} // namespace ptgn