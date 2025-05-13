#pragma once

#include "components/draw.h"
#include "components/drawable.h"
#include "math/vector3.h"
#include "rendering/api/color.h"
#include "serialization/serializable.h"

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

// Lights must be added to the LightManager to be drawn to the screen.
class PointLight : public Sprite, public Drawable<PointLight> {
public:
	PointLight() = default;

	// TODO: Add drawable constructor

	friend bool operator==(const PointLight& a, const PointLight& b) {
		return a.color_ == b.color_ && a.intensity_ == b.intensity_ &&
			   a.ambient_intensity_ == b.ambient_intensity_ &&
			   a.ambient_color_ == b.ambient_color_ && a.radius_ == b.radius_ &&
			   a.falloff_ == b.falloff_;
	}

	friend bool operator!=(const PointLight& a, const PointLight& b) {
		return !(a == b);
	}

	static void Draw(impl::RenderData& ctx, const Entity& entity);

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

	PTGN_SERIALIZER_REGISTER_NAMED(
		PointLight, KeyValue("intensity", intensity_), KeyValue("color", color_),
		KeyValue("ambient_intensity", ambient_intensity_),
		KeyValue("ambient_color", ambient_color_), KeyValue("radius", radius_),
		KeyValue("falloff", falloff_)
	)

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