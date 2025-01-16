#include "vfx/light.h"

#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/render_target.h"
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
	Draw({});
}

void Light::Draw(const RenderTarget& render_target) const {
	auto shader{ game.light.GetShader() };
	// TODO: Reduce shader binds when using light manager.
	shader.Bind();
	shader.SetUniform("u_LightPos", GetPosition());
	shader.SetUniform("u_LightIntensity", GetIntensity());
	// TODO: Fix this to use layer info.
	// shader.Draw(texture, {}, Matrix4{ 1.0f }, TextureInfo{ {}, {}, Flip::None, color_ });
}

void LightManager::Init() {
	target_		  = RenderTarget{ color::Transparent, BlendMode::Add };
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
	ForEachValue([&](const Light& light) { light.Draw(target_); });
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