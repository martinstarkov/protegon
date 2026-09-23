#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/noise.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_key.h"
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

enum class PaintGeneratorBrushShape : std::uint8_t {
	Circle,
	Square,
};
PTGN_REFLECT_ENUM(PaintGeneratorBrushShape);

enum class PaintGeneratorSourceKind : std::uint8_t {
	Single,
	WeightedSet,
	Checkerboard,
	Autotile,
	Noise,
};
PTGN_REFLECT_ENUM(PaintGeneratorSourceKind);

enum class PaintGeneratorCoverageMode : std::uint8_t {
	Solid,
	RandomDensity,
	RadialFalloff,
};
PTGN_REFLECT_ENUM(PaintGeneratorCoverageMode);

enum class PaintGeneratorTilePlacementMode : std::uint8_t {
	Grid,
	Tile,
};
PTGN_REFLECT_ENUM(PaintGeneratorTilePlacementMode);

enum class PaintGeneratorAutotileFormat : std::uint8_t {
	Classic15,
	Blob47,
	Subset16,
	DualGrid16,
	Wang16,
};
PTGN_REFLECT_ENUM(PaintGeneratorAutotileFormat);

struct PaintGeneratorTileSource {
	TextureKey texture{};
	std::array<V2_float, 4> texture_coordinates{
		V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f }, V2_float{ 0.0f, 1.0f }
	};
	V2_int pixel_size{ 32, 32 };
	Origin origin{ Origin::TopLeft };

	PTGN_REFLECT(PaintGeneratorTileSource, texture, texture_coordinates, pixel_size, origin)
};

struct PaintGeneratorWeightedTileEntry {
	PaintGeneratorTileSource source{};
	float weight{ 1.0f };

	PTGN_REFLECT(PaintGeneratorWeightedTileEntry, source, weight)
};

struct PaintGeneratorWeightedPrefabEntry {
	PrefabKey prefab{};
	float weight{ 1.0f };

	PTGN_REFLECT(PaintGeneratorWeightedPrefabEntry, prefab, weight)
};

/// @brief One output band in a procedural noise source. A Tile-layer generator uses tile, while
/// an Entity-layer generator uses prefab. Leaving both unset intentionally emits nothing.
struct PaintGeneratorNoiseThreshold {
	float minimum{};
	float maximum{ 1.0f };
	bool enabled{ true };
	PaintGeneratorSourceKind source_kind{ PaintGeneratorSourceKind::Single };
	std::optional<PaintGeneratorTileSource> tile{};
	std::optional<PrefabKey> prefab{};
	std::vector<PaintGeneratorWeightedTileEntry> weighted_tiles{};
	std::vector<PaintGeneratorWeightedPrefabEntry> weighted_prefabs{};
	Origin origin{ Origin::Center };

	PTGN_REFLECT(
		PaintGeneratorNoiseThreshold,
		minimum, maximum, enabled, source_kind, tile, prefab, weighted_tiles, weighted_prefabs, origin
	)
};

struct PaintGeneratorNoise {
	NoiseType type{ NoiseType::Perlin };
	int seed{ 1337 };
	float frequency{ 0.015f };
	int octaves{ 4 };
	float lacunarity{ 2.0f };
	float persistence{ 0.5f };
	V2_float offset{};
	std::vector<PaintGeneratorNoiseThreshold> thresholds{};

	PTGN_REFLECT(
		PaintGeneratorNoise,
		type,
		seed,
		frequency,
		octaves,
		lacunarity,
		persistence,
		offset,
		thresholds
	)
};

/// @brief Captured procedural source state. Persistent generators own a copy so changing the
/// editor's current Paint Recipe does not silently alter an existing generator.
struct PaintGeneratorRecipe {
	PaintGeneratorSourceKind source_kind{ PaintGeneratorSourceKind::Single };
	PaintGeneratorCoverageMode coverage{ PaintGeneratorCoverageMode::Solid };
	std::optional<PaintGeneratorTileSource> tile{};
	std::optional<PrefabKey> prefab{};
	std::vector<PaintGeneratorWeightedTileEntry> weighted_tiles{};
	std::vector<PaintGeneratorWeightedPrefabEntry> weighted_prefabs{};
	std::optional<PaintGeneratorTileSource> checker_tile{};
	std::optional<PrefabKey> checker_prefab{};
	PaintGeneratorAutotileFormat autotile_format{ PaintGeneratorAutotileFormat::DualGrid16 };
	std::vector<std::optional<PaintGeneratorTileSource>> autotile_tiles{ 16 };
	PaintGeneratorTilePlacementMode tile_placement{ PaintGeneratorTilePlacementMode::Tile };
	Origin tile_origin{ Origin::TopLeft };
	Origin entity_origin{ Origin::Center };
	float density{ 0.45f };
	float radial_inner{ 0.15f };
	float radial_outer{ 1.0f };
	float min_spacing{};
	bool avoid_exclusion_mask{ true };
	bool random_rotation{};
	float rotation_min{};
	float rotation_max{ 360.0f };
	bool random_scale{};
	float scale_min{ 0.8f };
	float scale_max{ 1.2f };
	PaintGeneratorNoise noise{};
	bool show_noise_preview{};
	bool show_generated_preview{ true };
	float noise_preview_alpha{ 0.45f };

	PTGN_REFLECT(
		PaintGeneratorRecipe,
		source_kind,
		coverage,
		tile,
		prefab,
		weighted_tiles,
		weighted_prefabs,
		checker_tile,
		checker_prefab,
		autotile_format,
		autotile_tiles,
		tile_placement,
		tile_origin,
		entity_origin,
		density,
		radial_inner,
		radial_outer,
		min_spacing,
		avoid_exclusion_mask,
		random_rotation,
		rotation_min,
		rotation_max,
		random_scale,
		scale_min,
		scale_max,
		noise,
		show_noise_preview,
		show_generated_preview,
		noise_preview_alpha
	)
};

struct PaintGeneratorStrokePoint {
	V2_float position{};
	int diameter{ 1 };

	PTGN_REFLECT(PaintGeneratorStrokePoint, position, diameter)
};

namespace impl {

/// @brief Persistent component that turns an ECS entity into a procedural PaintGenerator entity.
/// Geometry is stored in the generator's local world plane and the entity Transform offsets the
/// entire generator. The recipe is captured at creation time.
struct PaintGeneratorData {
	PaintGeneratorGeometry geometry{ PaintGeneratorGeometry::Rectangle };
	PaintGeneratorRecipe recipe{};

	/// @brief Optional target Tilemap for a generator living in a Tile layer.
	/// Entity-layer generators leave this unset and emit entity instances instead.
	std::optional<UUID> target_tilemap{};

	V2_float grid_size{ 32.0f, 32.0f };
	V2_float grid_offset{};
	V2_float start{};
	V2_float end{};

	PaintGeneratorBrushShape brush_shape{ PaintGeneratorBrushShape::Circle };
	int line_thickness{ 1 };
	int line_spacing{ 1 };
	PaintGeneratorAreaMode area_mode{ PaintGeneratorAreaMode::Fill };
	int area_thickness{ 1 };
	float random_fill_density{ 1.0f };

	std::vector<PaintGeneratorStrokePoint> stroke_points{};
	std::vector<V2_int> suppressed_cells{};
	bool enabled{ true };

	PTGN_REFLECT(
		PaintGeneratorData,
		geometry,
		recipe,
		target_tilemap,
		grid_size,
		grid_offset,
		start,
		end,
		brush_shape,
		line_thickness,
		line_spacing,
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
