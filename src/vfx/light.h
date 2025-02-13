#pragma once

#include "math/vector3.h"
#include "renderer/color.h"

namespace ptgn {

// Lights must be added to the LightManager to be drawn to the screen.
class PointLight {
public:
	bool operator==(const PointLight& o) const;
	bool operator!=(const PointLight& o) const;

	PointLight& SetIntensity(float intensity);
	[[nodiscard]] float GetIntensity() const;

	PointLight& SetColor(const Color& color);
	[[nodiscard]] Color GetColor() const;

	PointLight& SetAmbientIntensity(float ambient_intensity);
	[[nodiscard]] float GetAmbientIntensity() const;

	PointLight& SetAmbientColor(const Color& ambient_color);
	[[nodiscard]] Color GetAmbientColor() const;

	PointLight& SetRadius(float radius);
	[[nodiscard]] float GetRadius() const;

	PointLight& SetFalloff(float falloff);
	[[nodiscard]] float GetFalloff() const;

	// @return color_ normalized and without alpha value.
	[[nodiscard]] static V3_float GetShaderColor(const Color& color);

private:
	// Intensity of the light. Range: [0, 1].
	float intensity_{ 1.0f };

	// Color of the light.
	Color color_{ color::Cyan };

	// Intensity of the ambient light. Range: [0, 1].
	float ambient_intensity_{ 0.03f };

	// Color of the ambient light.
	Color ambient_color_{ color::Red };

	// Higher -> Light reaches further out from the center.
	float radius_{ 100.0f };

	// Higher -> Less light reaches the outer radius.
	float falloff_{ 2.0f };
};

} // namespace ptgn