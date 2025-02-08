// #pragma once
//
// #include "core/manager.h"
// #include "math/vector2.h"
// #include "math/vector3.h"
// #include "renderer/color.h"
// #include "renderer/render_target.h"
// #include "renderer/shader.h"
//
// namespace ptgn {
//
// class RenderTarget;
//
// namespace impl {
//
// class Game;
//
// } // namespace impl
//
// class LightManager;
//
//// Lights must be added to the LightManager to be drawn to the screen.
// class Light {
// public:
//	Light() = default;
//
//	Light(const V2_float& position, const Color& color) : position_{ position }, color_{ color } {
//		ambient_color_ = color;
//	}
//
//	Light(const V2_float& position, const Color& color, float intensity) :
//		position_{ position }, color_{ color }, intensity_{ intensity } {
//		ambient_color_ = color;
//	}
//
//	bool operator==(const Light& o) const;
//	bool operator!=(const Light& o) const;
//
//	void SetPosition(const V2_float& position);
//	[[nodiscard]] V2_float GetPosition() const;
//
//	void SetColor(const Color& color);
//	[[nodiscard]] Color GetColor() const;
//
//	void SetIntensity(float intensity);
//	[[nodiscard]] float GetIntensity() const;
//
//	// TODO: Move to private.
//	// TODO: Clean up and create getters and setters.
//	// TODO: Add increment functions.
//	V3_float attenuation_{ 0.0f, 0.0f, 0.0f };
//	// TODO: Assert radius isnt 0.
//	float radius_{ 50.0f };
//	float compression_{ 2.0f };
//	float ambient_intensity_{ 0.0f };
//	Color ambient_color_{ color::Red };
//
// private:
//	friend class LightManager;
//
//	// @return color_ normalized and without alpha value.
//	[[nodiscard]] V3_float GetShaderColor() const;
//
//	V2_float position_;
//	Color color_;
//	float intensity_{ 10.0f };
// };
//
// class LightManager : public MapManager<Light> {
// public:
//	LightManager()									 = default;
//	~LightManager() override						 = default;
//	LightManager(LightManager&&) noexcept			 = default;
//	LightManager& operator=(LightManager&&) noexcept = default;
//	LightManager(const LightManager&)				 = delete;
//	LightManager& operator=(const LightManager&)	 = delete;
//
//	// Draws all the lights in the light manager to the current render target.
//	// Note: If any lights exist in the light manager, this function will flush the renderer.
//	void Draw() const;
//
//	// Resets the light manager.
//	void Reset();
//
//	void SetBlur(bool blur);
//
//	[[nodiscard]] bool GetBlur() const;
//
// private:
//	friend class Light;
//	friend class impl::Game;
//
//	void Init();
//
//	[[nodiscard]] Shader GetShader() const;
//
//	RenderTarget target_;
//
//	Shader light_shader_;
//
//	bool blur_{ false };
// };
//
// } // namespace ptgn