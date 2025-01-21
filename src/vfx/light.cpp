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

void Light::DrawImpl() const {
	// Get valid render target.
	RenderTarget dest_target{ game.light.target_ };
	auto shader{ game.light.GetShader() };
	shader.Bind();
	shader.SetUniform("u_LightPos", GetPosition());
	shader.SetUniform("u_LightIntensity", GetIntensity());
	dest_target.GetTexture().Draw(
		dest_target.GetViewport(), TextureInfo{ color_ }, shader, dest_target.GetCamera()
	);
}

void Light::Draw() const {
	game.renderer.SetTemporaryRenderTarget(game.light.target_, [&]() {
		game.renderer.SetBlendMode(BlendMode::Blend);
		DrawImpl();
	});
}

void LightManager::Init() {
	target_		  = RenderTarget{ color::Transparent };
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
	game.renderer.SetTemporaryRenderTarget(target_, [&]() {
		game.renderer.SetBlendMode(BlendMode::Blend);
		ForEachValue([&](const Light& light) { light.DrawImpl(); });
	});
	game.renderer.SetTemporaryBlendMode(BlendMode::AddPremultiplied, [&]() { target_.Draw(); });
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