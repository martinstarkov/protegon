#include "vfx/light.h"

#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/render_texture.h"
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

void Light::Draw(const Texture& texture) const {
	auto shader{ game.light.GetShader() };
	shader.Bind();
	shader.SetUniform("u_LightPos", GetPosition());
	shader.SetUniform("u_LightColor", GetShaderColor());
	shader.SetUniform("u_LightIntensity", GetIntensity());
	game.draw.Shader(shader, texture, {}, BlendMode::Add);
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

	target_ = RenderTexture{ true };
}

void LightManager::Draw() {
	auto target{ game.draw.GetTarget() };
	game.draw.SetTarget(target_);
	ForEachValue([&](const auto& light) { light.Draw(target_.GetTexture()); });
	game.draw.SetTarget(RenderTexture{ game.window.GetSize() }, false);
	if (blur_) {
		game.draw.Shader(ScreenShader::GaussianBlur, target_.GetTexture());
	} else {
		game.draw.Shader(ScreenShader::Default, target_.GetTexture());
	}
	game.draw.SetTarget(target);
}

void LightManager::Reset() {
	MapManager::Reset();
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