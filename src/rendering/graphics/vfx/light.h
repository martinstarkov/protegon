#pragma once

#include "components/drawable.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "rendering/api/color.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class Shader;

namespace impl {

class RenderData;

struct LightProperties {
	// Intensity of the light. Range: [0, 1].
	float intensity{ 1.0f };

	// Color of the light.
	Color color{ color::Cyan };

	// Intensity of the ambient light. Range: [0, 1].
	float ambient_intensity{ 0.0f };

	// Color of the ambient light.
	Color ambient_color{ color::Transparent };

	// Higher -> Light reaches further out from the center.
	float radius{ 100.0f };

	// Higher -> Less light reaches the outer radius.
	float falloff{ 2.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		LightProperties, intensity, color, ambient_intensity, ambient_color, radius, falloff
	)
};

} // namespace impl

// Lights must be added to the LightManager to be drawn to the screen.
class PointLight : public Entity, public Drawable<PointLight> {
public:
	PointLight() = default;

	PointLight(const Entity& entity);

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

	// @return color normalized and without alpha value.
	[[nodiscard]] static V3_float GetShaderColor(const Color& color);

private:
	friend PointLight CreatePointLight(
		Scene& scene, const V2_float& position, float radius, const Color& color, float intensity,
		float falloff
	);

	static void SetUniform(Entity entity, const Shader& shader);
};

// @param position Starting point of the light.
// @param radius The higher the radius, the further light reaches out from the center.
// @param color Color of the light.
// @param intensity Intensity of the light source. Range: [0, 1].
// @param falloff The higher the value, the Less light reaches the outer radius.
PointLight CreatePointLight(
	Scene& scene, const V2_float& position, float radius, const Color& color,
	float intensity = 0.5f, float falloff = 2.0f
);

} // namespace ptgn