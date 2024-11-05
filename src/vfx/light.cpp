#include "vfx/light.h"

#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"

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

void Light::Draw() const {
	auto shader{ game.light.GetShader() };
	shader.Bind();
	shader.SetUniform("u_LightPos", GetPosition());
	shader.SetUniform("u_LightColor", GetShaderColor());
	shader.SetUniform("u_LightIntensity", GetIntensity());
	game.draw.Shader(shader, {}, {}, Origin::Center, game.light.GetBlendMode());
	game.draw.Flush();
}

void LightManager::Init() {
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
	ForEachValue([&](const auto& light) { light.Draw(); });
	if (blur_) {
		game.draw.Shader(ScreenShader::Blur, BlendMode::Add);
	}
}

void LightManager::Reset() {
	MapManager::Reset();
	blur_		= false;
	blend_mode_ = BlendMode::Add;
}

void LightManager::SetBlur(bool blur) {
	blur_ = blur;
}

bool LightManager::GetBlur() const {
	return blur_;
}

void LightManager::SetBlendMode(BlendMode blend_mode) {
	blend_mode_ = blend_mode;
}

BlendMode LightManager::GetBlendMode() const {
	return blend_mode_;
}

Shader LightManager::GetShader() const {
	return light_shader_;
}

} // namespace ptgn