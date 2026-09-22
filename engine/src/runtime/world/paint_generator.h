#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/world/entity_layer.h"
#include "runtime/world/tilemap.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
enum class PaintGeneratorGeometry : std::uint8_t {
	Rectangle,
	Line,
	BrushStroke,
	Infinite,
};
PTGN_REFLECT_ENUM(PaintGeneratorGeometry);

enum class PaintGeneratorAreaMode : std::uint8_t {
	Fill,
	Outline,
	Corners,
	RandomFill,
};
PTGN_REFLECT_ENUM(PaintGeneratorAreaMode);

struct PaintGeneratorStrokePoint {
	V2_float position{};
	int diameter{ 1 };

	PTGN_REFLECT(PaintGeneratorStrokePoint, position, diameter)
};

namespace impl {

/// @brief Persistent component that turns an ECS entity into a procedural PaintGenerator entity.
///
/// This file intentionally establishes generator identity/geometry/ownership only. Paint-source
/// recipes (single/weighted/checkerboard/autotile/noise) can be layered onto this entity model
/// without changing the Scene Hierarchy paradigm again.
struct PaintGeneratorData {
	PaintGeneratorGeometry geometry{ PaintGeneratorGeometry::Rectangle };

	/// @brief Optional target Tilemap for a generator living in a Tile layer.
	/// Entity-layer generators leave this unset and emit entity instances instead.
	std::optional<UUID> target_tilemap{};

	V2_float grid_size{ 32.0f, 32.0f };
	V2_float grid_offset{};
	V2_float start{};
	V2_float end{};

	int line_thickness{ 1 };
	PaintGeneratorAreaMode area_mode{ PaintGeneratorAreaMode::Fill };
	int area_thickness{ 1 };
	float random_fill_density{ 1.0f };

	std::vector<PaintGeneratorStrokePoint> stroke_points{};
	std::vector<V2_int> suppressed_cells{};
	bool enabled{ true };

	PTGN_REFLECT(
		PaintGeneratorData,
		geometry,
		target_tilemap,
		grid_size,
		grid_offset,
		start,
		end,
		line_thickness,
		area_mode,
		area_thickness,
		random_fill_density,
		stroke_points,
		suppressed_cells,
		enabled
	)
};

} // namespace impl

class PaintGenerator : public Entity {
public:
	PaintGenerator() = default;
	explicit PaintGenerator(Entity entity);

	[[nodiscard]] const impl::PaintGeneratorData& GetData() const;
	[[nodiscard]] impl::PaintGeneratorData& GetData();

	PaintGenerator& SetGeometry(PaintGeneratorGeometry geometry);
	PaintGenerator& SetGrid(V2_float size, V2_float offset = {});
	PaintGenerator& SetEnabled(bool enabled);

	/// @brief Sets the target tilemap for a generator in a Tile layer.
	/// @return False if target is from another scene, is not a Tilemap, or is not in the same layer.
	bool SetTargetTilemap(std::optional<Tilemap> target);

	[[nodiscard]] bool IsSuppressed(V2_int cell) const;
	bool SetSuppressed(V2_int cell, bool suppressed = true);
	void ClearSuppressions();
};

[[nodiscard]] bool IsPaintGenerator(Entity entity);

/// @brief Creates a generator entity in either an Entity or Tile layer.
[[nodiscard]] PaintGenerator CreatePaintGenerator(
	Scene& scene,
	SceneLayerId layer,
	Tag tag = Tag{ "Generator" }
);

} // namespace ptgn
