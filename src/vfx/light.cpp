#include "vfx/light.h"

#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "utility/assert.h"

namespace ptgn {

PointLight& PointLight::SetIntensity(float intensity) {
	intensity_ = intensity;
	return *this;
}

float PointLight::GetIntensity() const {
	return intensity_;
}

PointLight& PointLight::SetColor(const Color& color) {
	color_ = color;
	return *this;
}

Color PointLight::GetColor() const {
	return color_;
}

PointLight& PointLight::SetAmbientIntensity(float ambient_intensity) {
	ambient_intensity_ = ambient_intensity;
	return *this;
}

float PointLight::GetAmbientIntensity() const {
	return ambient_intensity_;
}

PointLight& PointLight::SetAmbientColor(const Color& ambient_color) {
	ambient_color_ = ambient_color;
	return *this;
}

Color PointLight::GetAmbientColor() const {
	return ambient_color_;
}

PointLight& PointLight::SetRadius(float radius) {
	PTGN_ASSERT(radius > 0.0f, "Point light radius must be above 0");
	radius_ = radius;
	return *this;
}

float PointLight::GetRadius() const {
	return radius_;
}

PointLight& PointLight::SetFalloff(float falloff) {
	PTGN_ASSERT(falloff >= 0.0f, "Point light falloff must be above or equal to 0");
	falloff_ = falloff;
	return *this;
}

float PointLight::GetFalloff() const {
	return falloff_;
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