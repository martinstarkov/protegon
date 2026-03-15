#pragma once

#include <optional>

#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class DrawContext;
class Scene;

namespace impl {

struct LightData {
	/// @brief Intensity of the light. Range: [0, 1].
	float intensity{ 1.0f };

	/// @brief Intensity of the ambient light. Range: [0, 1].
	float ambient_intensity{ 0.0f };

	/// @brief Color of the ambient light.
	Color ambient_color{ color::Transparent };

	/// @brief Higher -> Less light reaches the outer radius.
	float falloff{ 2.0f };

	/// @brief Angle of the light cone in radians. Range: [0.0, 2pi). 0.0 means no light is drawn,
	/// 2pi means the light is a point light and has no cone. If std::nullopt, the light is a
	/// point light.
	std::optional<float> cone_angle;

	// TODO: Fix serialization of cone angle.
	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		LightData, intensity, ambient_intensity, ambient_color, falloff
	)
};

} // namespace impl

class Light : public Entity {
public:
	Light() = default;
	explicit Light(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

	Light& SetIntensity(float intensity);
	[[nodiscard]] float GetIntensity() const;

	Light& SetColor(Color color);
	[[nodiscard]] Color GetColor() const;

	Light& SetAmbientIntensity(float ambient_intensity);
	[[nodiscard]] float GetAmbientIntensity() const;

	Light& SetAmbientColor(Color ambient_color);
	[[nodiscard]] Color GetAmbientColor() const;

	Light& SetRadius(float radius);
	[[nodiscard]] float GetRadius() const;

	Light& SetFalloff(float falloff);
	[[nodiscard]] float GetFalloff() const;

	/// @param cone_angle Angle of the light cone in degrees. If std::nullopt, the light is a
	/// point light. Range: [0.0, 360.0]. 0.0 means no light is drawn, 360.0 means the light is a
	/// point light and has no cone.
	Light& SetConeAngle(std::optional<float> cone_angle);

	/// @return Cone angle in degrees, if it has been set. Range: [0.0, 360.0].
	[[nodiscard]] std::optional<float> GetConeAngle() const;

private:
	static void SetUniform(DrawContext& renderer, Entity entity);
};

PTGN_REGISTER_DRAWABLE(Light);

/// @param position Starting point of the light.
/// @param radius The higher the radius, the further light reaches out from the center.
/// @param color Color of the light.
/// @param cone_angle Angle of the light cone in degrees. If std::nullopt, the light is a
/// point light. Range: [0.0, 360.0]. 0.0 means no light is drawn, 360.0 means the light is a point
/// light and has no cone.
/// @param direction_angle Angle of the light direction in degrees. 0.0 means pointing to the right.
/// @param intensity Intensity of the light source. Range: [0, 1].
/// @param falloff The higher the value, the Less light reaches the outer radius.
Light CreateLight(
	Scene& scene, V2_float position, float radius, Color color,
	std::optional<float> cone_angle = {}, float direction_angle = 0.0f, float intensity = 0.5f,
	float falloff = 2.0f
);

} // namespace ptgn