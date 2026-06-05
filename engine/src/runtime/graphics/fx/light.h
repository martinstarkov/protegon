#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "renderer/resources/shader.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/render_queue.h"
#include "serialization/serialize.h"

namespace ptgn {

class DrawContext;
class Scene;

struct LightProperties {
	/// @brief Radius of the light. The higher the radius, the further light reaches out from the
	/// center.
	float radius{ 100.0f };

	/// @brief Color of the light.
	Color color{ color::Red };

	/// @brief Angle of the light cone. If std::nullopt, the light is a
	/// point light. Range: [0.0, 360.0]. 0.0 means no light is drawn,
	/// 360.0 means the light is a point light and has no cone.
	std::optional<Degrees> cone_angle;

	/// @brief Initial angle of the light direction. 0.0 means pointing to the right.
	/// Range: [0.0, 360.0]. Only applies to lights with a cone angle.
	Degrees direction_angle{ 0.0f };

	/// @brief Intensity of the light source. Range: [0, 1].
	float intensity{ 0.5f };

	/// @brief Falloff of the light. The higher the value, the less light reaches the outer radius.
	float falloff{ 2.0f };

	PTGN_SERIALIZE(LightProperties, radius, color, cone_angle, direction_angle, intensity, falloff)
};

namespace impl {

void BuildLightVisibilityPolygons(Scene&, std::span<const CameraRenderBucket> buckets);

void DrawLightVisibilityDebug(Scene& scene);

struct LightData {
	/// @brief Intensity of the light. Range: [0, 1].
	float intensity{ 1.0f };

	/// @brief Intensity of the ambient light. Range: [0, 1].
	float ambient_intensity{ 0.0f };

	/// @brief Color of the ambient light.
	Color ambient_color{ color::Transparent };

	/// @brief Higher -> Less light reaches the outer radius.
	float falloff{ 2.0f };

	/// @brief Angle of the light cone. Range: [0.0, 2pi]. 0.0 means no light is drawn,
	/// 2pi means the light is a point light and has no cone. If std::nullopt, the light is a
	/// point light.
	std::optional<Radians> cone_angle;

	PTGN_SERIALIZE(LightData, intensity, ambient_intensity, ambient_color, falloff, cone_angle)
};

struct ShadowCaster {
	bool casts_shadows{ true };
	bool masks_light_inside{ true };

	PTGN_SERIALIZE(ShadowCaster, casts_shadows, masks_light_inside)
};

struct ShadowMaskInterior {
	// World-space vertices of the occluder's filled body.
	std::vector<V2_float> vertices;

	// If true, the occluder's own interior remains black in the light mask.
	// If false, the occluder's interior is painted back into the mask.
	bool masks_light_inside{ true };
};

struct VisibilityPolygon {
	// World-space visibility polygon vertices.
	std::vector<V2_float> vertices;

	// World-space filled occluder bodies used by masks_light_inside.
	std::vector<ShadowMaskInterior> occluder_interiors;
};

} // namespace impl

class Light : public Entity {
public:
	Light() = default;
	explicit Light(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	Light& SetIntensity(float intensity);
	float GetIntensity() const;

	Light& SetColor(Color color);
	Color GetColor() const;

	Light& SetAmbientIntensity(float ambient_intensity);
	float GetAmbientIntensity() const;

	Light& SetAmbientColor(Color ambient_color);
	Color GetAmbientColor() const;

	Light& SetRadius(float radius);
	float GetRadius() const;

	Light& SetFalloff(float falloff);
	float GetFalloff() const;

	/// @param cone_angle Angle of the light cone. If std::nullopt, the light is a
	/// point light. Range: [0.0, 360.0]. 0.0 means no light is drawn, 360.0 means the light is a
	/// point light and has no cone.
	Light& SetConeAngle(std::optional<Degrees> cone_angle);

	/// @return Cone angle, if it has been set. Range: [0.0, 360.0].
	std::optional<Degrees> GetConeAngle() const;

	Light& SetLightProperties(const LightProperties& properties);
	LightProperties GetLightProperties() const;

private:
	std::array<UniformWrite, 9> GetUniforms() const;
};

PTGN_REGISTER_DRAWABLE(Light);

/// @param position Starting point of the light.
/// @param properties Optional properties of the light. If not provided, default properties will be
/// used.
Light CreateLight(Scene& scene, V2_float position = {}, const LightProperties& properties = {});

/// @brief Marks an entity as a shadow occluder.
/// @param entity Entity that should cast shadows.
/// @param casts_shadows If false, the entity keeps the component but is ignored by visibility
/// solving.
/// @param masks_light_inside If true, the entity's own interior remains unlit in the light mask.
Entity SetOccluder(Entity entity, bool casts_shadows = true, bool masks_light_inside = true);

} // namespace ptgn