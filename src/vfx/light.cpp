#include "vfx/light.h"

#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"

namespace ptgn {

void Light::SetPosition(const V2_float& position) {
	position_ = position;
}

V2_float Light::GetPosition() const {
	return position_;
}

void Light::SetColor(const Color& color) {
	color_ = color;
}

Color Light::GetColor() const {
	return color_;
}

void Light::SetIntensity(float intensity) {
	intensity_ = intensity;
}

float Light::GetIntensity() const {
	return intensity_;
}

V3_float Light::GetShaderColor() const {
	V4_float n{ color_.Normalized() };
	return { n.x, n.y, n.z };
}

bool Light::operator==(const Light& o) const {
	return color_ == o.color_ && intensity_ == o.intensity_ && position_ == o.position_;
}

bool Light::operator!=(const Light& o) const {
	return !(*this == o);
}

void LightManager::Init() {
	target_		  = RenderTarget{ color::Transparent, BlendMode::Blend };
	light_shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
		},
		ShaderSource{
#include PTGN_SHADER_PATH(lighting.frag)
		}
	);
}

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

void LightManager::Reset() {
	VectorAndMapManager<Light>::Reset();
	blur_ = false;
}

void LightManager::SetBlur(bool blur) {
	blur_ = blur;
}

bool LightManager::GetBlur() const {
	return blur_;
}

Shader LightManager::GetShader() const {
	return light_shader_;
}

} // namespace ptgn