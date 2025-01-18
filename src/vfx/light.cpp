#include "vfx/light.h"

#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/layer_info.h"
#include "renderer/render_target.h"
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
	// Get valid render target.
	RenderTarget dest_target{ game.light.target_ };
	auto shader{ game.light.GetShader() };
	shader.Bind();
	shader.SetUniform("u_LightPos", GetPosition());
	shader.SetUniform("u_LightIntensity", GetIntensity());
	shader.Draw(
		dest_target.GetTexture(), {}, dest_target.GetCamera().GetPrimary().GetViewProjection(),
		TextureInfo{ color_ }, dest_target
	);
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

void LightManager::Draw() {
	ForEachValue([&](const Light& light) { light.Draw(); });
	auto blend_mode{ game.renderer.GetBlendMode() };
	game.renderer.SetBlendMode(BlendMode::AddPremultiplied);
	target_.Draw();
	game.renderer.SetBlendMode(blend_mode);
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