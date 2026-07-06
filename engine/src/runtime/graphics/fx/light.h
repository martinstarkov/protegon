#pragma once

#include <optional>
#include <span>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

class DrawContext;
class Scene;

struct LightConfig {
	/// @brief Color of the light.
	Color color{ color::Red };

	/// @brief Intensity of the light source. Range: [0, 1].
	float intensity{ 0.5f };

	/// @brief Falloff of the light. The higher the value, the less light reaches the outer radius.
	float falloff{ 2.0f };

	/// @brief Radius of the light. The higher the radius, the further light reaches out from the
	/// center.
	float radius{ 100.0f };

	/// @brief Angle of the light cone. If std::nullopt, the light is a
	/// point light. Range: [0.0, 360.0]. 0.0 means no light is drawn,
	/// 360.0 means the light is a point light and has no cone.
	std::optional<Degrees> cone_angle;

	/// @brief Initial angle of the light direction. 0.0 means pointing to the right.
	/// Range: [0.0, 360.0]. Only applies to lights with a cone angle.
	Degrees direction_angle{ 0.0f };

	/// @brief Color of the ambient light.
	Color ambient_color{ color::Transparent };

	/// @brief Intensity of the ambient light. Range: [0, 1].
	float ambient_intensity{ 0.0f };

	PTGN_SERIALIZE(
		LightConfig, color, intensity, falloff, radius, cone_angle, direction_angle, ambient_color,
		ambient_intensity
	)
};

namespace impl {

struct EntityRenderCommand;

void UpdateLightVisibilityPolygons(
	std::vector<impl::EntityRenderCommand>& commands, std::span<const V2_float> camera_vertices
);

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

	Light& Intensity(float intensity);
	Light& Color(ptgn::Color color);
	Light& AmbientIntensity(float ambient_intensity);
	Light& AmbientColor(ptgn::Color ambient_color);
	Light& Radius(float radius);
	Light& Falloff(float falloff);
	Light& Config(const LightConfig& config);

	/// @param cone_angle Angle of the light cone. If std::nullopt, the light is a
	/// point light. Range: [0.0, 360.0]. 0.0 means no light is drawn, 360.0 means the light is a
	/// point light and has no cone.
	Light& ConeAngle(std::optional<Degrees> cone_angle);

	LightConfig GetConfig() const;
};

PTGN_REGISTER_DRAWABLE(Light);

Light CreateLight(Scene& scene, Transform transform = {}, const LightConfig& config = {});

/// @brief Marks an entity as a shadow occluder.
/// @param entity Entity that should cast shadows.
/// @param casts_shadows If false, the entity keeps the component but is ignored by visibility
/// solving.
/// @param masks_light_inside If true, the entity's own interior remains unlit in the light mask.
Entity SetOccluder(Entity entity, bool casts_shadows = true, bool masks_light_inside = true);

} // namespace ptgn