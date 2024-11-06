#pragma once

#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/color.h"
#include "renderer/shader.h"

namespace ptgn {

namespace impl {

class Game;

} // namespace impl

class LightManager;

class Light {
public:
	Light() = default;

	Light(const V2_float& position, const Color& color, float intensity = 10.0f) :
		position_{ position }, color_{ color }, intensity_{ intensity } {}

	// Will flush the renderer.
	void Draw() const;

	void SetPosition(const V2_float& position);
	[[nodiscard]] V2_float GetPosition() const;

	void SetColor(const Color& color);
	[[nodiscard]] Color GetColor() const;

	void SetIntensity(float intensity);
	[[nodiscard]] float GetIntensity() const;

private:
	friend class LightManager;

	// @return color_ normalized and without alpha value.
	[[nodiscard]] V3_float GetShaderColor() const;

	V2_float position_;
	Color color_;
	float intensity_{ 10.0f };
};

class LightManager : public MapManager<Light> {
public:
	using MapManager::MapManager;

	// Will flush the renderer.
	void Draw() const;

	void Reset();

	void SetBlur(bool blur);
	[[nodiscard]] bool GetBlur() const;

	void SetBlendMode(BlendMode blend_mode);
	[[nodiscard]] BlendMode GetBlendMode() const;

private:
	friend class Light;
	friend class impl::Game;

	void Init();

	[[nodiscard]] Shader GetShader() const;

	Shader light_shader_;
	BlendMode blend_mode_{ BlendMode::Add };
	bool blur_{ false };
};

} // namespace ptgn