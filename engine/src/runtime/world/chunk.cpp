#include "runtime/world/chunk.h"

#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/world/autotile.h"
#include "runtime/world/paint_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <map>
#include <limits>
#include <queue>
#include <ranges>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/time.h"
#include "core/math/transform.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/bounding_aabb.h"
#include "core/math/geometry/rect.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/graphics/visible.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/draw.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

/// Change to false to use the original one-Collider-per-tile behavior.
constexpr bool kMergeTileColliders = true;

int FloorToInt(float value) {
	return static_cast<int>(std::floor(value));
}

int CeilToInt(float value) {
	return static_cast<int>(std::ceil(value));
}

std::string CellKey(std::string_view prefix, V2_int cell) {
	return std::string{ prefix } + ":" + std::to_string(cell.x) + ":" +
		   std::to_string(cell.y);
}

json EntityState(Entity entity) {
	return json{ SerializeEntity(entity, { .include_uuid = false, .include_children = true }) };
}

// Apply all persistent changes, including removed components and changed child trees.
// An override replaces the generated subtree, but does not replace the generated root UUID.
void ApplyEntitySnapshot(Scene& scene, Entity entity, const SerializedEntity& saved) {
	for (const auto& component : ComponentRegistry::Components()) {
		if (impl::IsEntityMetadataComponent(component) || !component.Has(entity)) {
			continue;
		}

		bool should_exist{ component.is_empty ?
			std::ranges::contains(saved.tags, component.name) :
			(!component.serializable || !component.deserializable ||
			 saved.components.contains(component.name)) };
		if (!should_exist) {
			component.Remove(entity);
		}
	}

	DeserializeEntity(saved, entity);
	if (HasChildren(entity)) {
		std::vector<Entity> old_children{ GetChildren(entity) };
		for (Entity child : old_children) {
			if (child) {
				child.Destroy();
			}
		}
		ClearChildren(entity);
	}

	for (const auto& saved_child : saved.children) {
		Entity child{ scene.CreateEntity(Tag{ saved_child.tag }) };
		SetParent(child, entity);
		ApplyEntitySnapshot(scene, child, saved_child);
	}
}

json TileState(const std::optional<TilemapTile>& tile) {
	return tile ? json{ *tile } : json{};
}

void PutDelta(json& changes, std::string_view group, const std::string& key, json state, json baseline) {
	if (state == baseline) {
		if (changes.contains(group)) {
			changes[std::string{ group }].erase(key);
		}
	} else {
		changes[std::string{ group }][key] = std::move(state);
	}
}

bool EmptyDelta(const json& changes) {
	return (!changes.contains("entities") || changes.at("entities").empty()) &&
           (!changes.contains("tiles") || changes.at("tiles").empty()) &&
           (!changes.contains("spawned") || changes.at("spawned").empty());
}

bool HasOverride(const json& changes, std::string_view group, const std::string& key) {
	return changes.contains(group) && changes.at(group).contains(key);
}

std::optional<TilemapTile> ReadTile(const json& value) {
	return value.is_null() ? std::nullopt : std::optional<TilemapTile>{ value.get<TilemapTile>() };
}

void ApplyTile(Tilemap tilemap, V2_int cell, const std::optional<TilemapTile>& tile) {
	if (tile) {
		tilemap.SetTile(*tile);
	} else {
		tilemap.EraseTile(cell);
	}
}

path StoragePath(path filepath) {
#if defined(__EMSCRIPTEN__)
	if (filepath.empty()) {
		return {};
	}
	// MEMFS is Emscripten's default writable filesystem. No database mount,
	// JavaScript shim or asynchronous hydration is required. Reloading loses the save.
	return (path{ "/tmp/protegon_chunks" } /
		filepath.relative_path()).lexically_normal();
#else
	return (filepath.is_absolute() ? filepath : GetRuntimeRoot() / filepath).lexically_normal();
#endif
}

// Write a complete JSON document atomically, including on platforms where rename does not
// replace an existing file. A .bak file is recoverable after a crash during replacement.
bool CommitJson(const path& filepath, const json& data) {
	std::error_code error;
	if (!filepath.parent_path().empty()) {
		fs::create_directories(filepath.parent_path(), error);
		if (error) {
			return false;
		}
	}
	path temporary{ filepath.string() + ".tmp" };
	{
		std::ofstream output{ temporary, std::ios::binary | std::ios::trunc };
		if (!output) {
			return false;
		}
		output << data.dump(2);
		output.flush();
		if (!output) {
			return false;
		}
	}
	fs::rename(temporary, filepath, error);
	if (error && fs::exists(filepath)) {
		path backup{ filepath.string() + ".bak" };
		std::error_code ignored;
		fs::remove(backup, ignored);
		error.clear();
		fs::rename(filepath, backup, error);
		if (!error) {
			fs::rename(temporary, filepath, error);
			if (error) {
				fs::rename(backup, filepath, ignored);
			} else {
				fs::remove(backup, ignored);
			}
		}
	}
	if (error) {
		fs::remove(temporary, error);
		return false;
	}
	return true;
}

std::uint32_t PaintHash(V2_int cell, std::uint32_t seed = 0x9e3779b9u) {
    std::uint32_t x{ static_cast<std::uint32_t>(cell.x) };
    std::uint32_t y{ static_cast<std::uint32_t>(cell.y) };
    std::uint32_t h{ seed ^ (x * 0x85ebca6bu) ^ (y * 0xc2b2ae35u) };
    h ^= h >> 16u;
    h *= 0x7feb352du;
    h ^= h >> 15u;
    h *= 0x846ca68bu;
    h ^= h >> 16u;
    return h;
}

float PaintRandom(V2_int cell) {
    return static_cast<float>(PaintHash(cell) & 0x00ffffffu) /
        static_cast<float>(0x01000000u);
}

V2_int PaintCell(V2_float point, const impl::PaintGeneratorData& data) {
    return { FloorToInt((point.x - data.grid_offset.x) / data.grid_size.x),
        FloorToInt((point.y - data.grid_offset.y) / data.grid_size.y) };
}

bool PaintGeometryIncludes(const impl::PaintGeneratorData& data, V2_int cell) {
    if (data.geometry == PaintGeneratorGeometry::Infinite) {
        return true;
    }
    if (data.geometry == PaintGeneratorGeometry::Rectangle) {
        V2_int a{ PaintCell(data.start, data) };
        V2_int b{ PaintCell(data.end, data) };
        int min_x{ std::min(a.x, b.x) };
        int max_x{ std::max(a.x, b.x) };
        int min_y{ std::min(a.y, b.y) };
        int max_y{ std::max(a.y, b.y) };
        if (cell.x < min_x || cell.x > max_x || cell.y < min_y || cell.y > max_y) {
            return false;
        }
        int left{ cell.x - min_x };
        int right{ max_x - cell.x };
        int top{ cell.y - min_y };
        int bottom{ max_y - cell.y };
        switch (data.area_mode) {
            case PaintGeneratorAreaMode::Fill: return true;
            case PaintGeneratorAreaMode::RandomFill:
                return PaintRandom(cell) <= data.random_fill_density;
            case PaintGeneratorAreaMode::Outline:
                return left < data.area_thickness || right < data.area_thickness ||
                    top < data.area_thickness || bottom < data.area_thickness;
            case PaintGeneratorAreaMode::Corners:
                return ((left < data.area_thickness || right < data.area_thickness) &&
                    (top < data.area_thickness || bottom < data.area_thickness));
        }
    }
    if (data.geometry == PaintGeneratorGeometry::Line) {
        V2_int a{ PaintCell(data.start, data) };
        V2_int b{ PaintCell(data.end, data) };
        int dx{ std::abs(b.x - a.x) };
        int dy{ -std::abs(b.y - a.y) };
        int sx{ a.x < b.x ? 1 : -1 };
        int sy{ a.y < b.y ? 1 : -1 };
        int error{ dx + dy };
        int step{};
        for (;;) {
            if (step % std::max(1, data.line_spacing) == 0) {
                int half{ std::max(1, data.line_thickness) / 2 };
                if (cell.x >= a.x - half && cell.x <= a.x + half &&
                    cell.y >= a.y - half && cell.y <= a.y + half) {
                    return true;
                }
            }
            if (a == b) {
                break;
            }
            int e2{ 2 * error };
            if (e2 >= dy) {
                error += dy;
                a.x += sx;
            }
            if (e2 <= dx) {
                error += dx;
                a.y += sy;
            }
            ++step;
        }
        return false;
    }
    if (data.geometry == PaintGeneratorGeometry::BrushStroke) {
        for (const auto& point : data.stroke_points) {
            V2_int center{ PaintCell(point.position, data) };
            int diameter{ std::max(1, point.diameter) };
            int low{ -(diameter / 2) };
            int high{ low + diameter - 1 };
            auto inside = [&](V2_int candidate) {
                V2_int d{ candidate - center };
                if (d.x < low || d.x > high || d.y < low || d.y > high) {
                    return false;
                }
                if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
                    float shift{ (diameter % 2) == 0 ? 0.5f : 0.0f };
                    float x{ static_cast<float>(d.x) + shift };
                    float y{ static_cast<float>(d.y) + shift };
                    return x * x + y * y <= static_cast<float>(diameter * diameter) * 0.25f;
                }
                return true;
            };
            if (inside(cell)) {
                return true;
            }
            if (data.recipe.source_kind == PaintGeneratorSourceKind::Autotile &&
                data.recipe.autotile_format == PaintGeneratorAutotileFormat::Classic15 &&
                (inside(cell - V2_int{ 1, 0 }) || inside(cell - V2_int{ 0, 1 }) ||
                 inside(cell - V2_int{ 1, 1 }))) {
                return true;
            }
        }
    }
    return false;
}

bool PaintIncludes(const impl::PaintGeneratorData& data, V2_int cell) {
    if (!data.enabled || !data.stream_at_runtime.value_or(true) ||
        std::ranges::contains(data.suppressed_cells, cell) ||
        !PaintGeometryIncludes(data, cell)) {
        return false;
    }
    if (data.recipe.coverage == PaintGeneratorCoverageMode::RandomDensity) {
        return PaintRandom(cell) <= data.recipe.density;
    }
    if (data.recipe.coverage == PaintGeneratorCoverageMode::RadialFalloff) {
        // No finite radius exists for infinite generators. Their density is uniform.
        if (data.geometry != PaintGeneratorGeometry::Rectangle) {
            return PaintRandom(cell) <= data.recipe.density;
        }
        V2_int a{ PaintCell(data.start, data) };
        V2_int b{ PaintCell(data.end, data) };
        V2_int first{ Min(a, b) };
        V2_int last{ Max(a, b) };
        V2_float center{ (V2_float{ first } + V2_float{ last } +
            V2_float{ 1.0f, 1.0f }) * 0.5f };
        V2_float span{ std::max(1.0f, static_cast<float>(last.x - first.x + 1)) * 0.5f,
            std::max(1.0f, static_cast<float>(last.y - first.y + 1)) * 0.5f };
        V2_float p{ static_cast<float>(cell.x) + 0.5f,
            static_cast<float>(cell.y) + 0.5f };
        V2_float delta{ (p.x - center.x) / span.x, (p.y - center.y) / span.y };
        float distance{ std::min(1.0f, delta.Magnitude()) };
        float inner{ std::clamp(data.recipe.radial_inner, 0.0f, 1.0f) };
        float outer{ std::max(inner + 0.001f,
            std::clamp(data.recipe.radial_outer, 0.0f, 1.0f)) };
        float t{ std::clamp((distance - inner) / (outer - inner), 0.0f, 1.0f) };
        return PaintRandom(cell) <= (1.0f - t) * data.recipe.density;
    }
    return true;
}

template <class Entry>
std::optional<std::size_t> PaintWeightedChoice(const std::vector<Entry>& entries, V2_int cell) {
    float sum{};
    for (const auto& entry : entries) {
        sum += std::max(0.0f, entry.weight);
    }
    if (sum <= 0.0f) {
        return std::nullopt;
    }
    float chosen{ PaintRandom(cell) * sum };
    for (std::size_t i{}; i < entries.size(); ++i) {
        chosen -= std::max(0.0f, entries[i].weight);
        if (chosen <= 0.0f) {
            return i;
        }
    }
    return entries.size() - 1;
}

float PaintNoise(const PaintGeneratorNoise& noise, V2_float position) {
    float frequency{ std::max(0.00001f, noise.frequency) };
    float amplitude{ 1.0f };
    float total{};
    float sum{};
    for (int octave{}; octave < std::clamp(noise.octaves, 1, 12); ++octave) {
        float x{ (position.x + noise.offset.x) * frequency };
        float y{ (position.y + noise.offset.y) * frequency };
        int seed{ noise.seed + octave * 1013 };
        float sample{};
        switch (noise.type) {
            case NoiseType::Perlin: sample = PerlinNoise::GetValue(x, y, seed, 1.0f); break;
            case NoiseType::Simplex: sample = SimplexNoise::GetValue(x, y, seed, 1.0f); break;
            case NoiseType::Value: sample = ValueNoise::GetValue(x, y, seed, 1.0f); break;
        }
        total += std::clamp(sample * 0.5f + 0.5f, 0.0f, 1.0f) * amplitude;
        sum += amplitude;
        frequency *= std::max(1.0f, noise.lacunarity);
        amplitude *= std::clamp(noise.persistence, 0.0f, 1.0f);
    }
    return sum > 0.0f ? std::clamp(total / sum, 0.0f, 1.0f) : 0.0f;
}

struct PaintSource {
    std::optional<PaintGeneratorTileSource> tile;
    std::optional<PrefabKey> prefab;
    Origin origin{ Origin::Center };
};

PaintSource SelectPaintSource(const impl::PaintGeneratorData& data, V2_int cell,
                              V2_float center, bool tile_mode) {
    const auto& recipe{ data.recipe };
    PaintSource result{ .origin = recipe.entity_origin };
    switch (recipe.source_kind) {
        case PaintGeneratorSourceKind::Single:
            result.tile = recipe.tile;
            result.prefab = recipe.prefab;
            break;
        case PaintGeneratorSourceKind::WeightedSet:
            if (tile_mode) {
                auto i{ PaintWeightedChoice(recipe.weighted_tiles, cell) };
                if (i) {
                    result.tile = recipe.weighted_tiles[*i].source;
                }
            } else {
                auto i{ PaintWeightedChoice(recipe.weighted_prefabs, cell) };
                if (i) {
                    result.prefab = recipe.weighted_prefabs[*i].prefab;
                }
            }
            break;
        case PaintGeneratorSourceKind::Checkerboard:
            if ((cell.x + cell.y) & 1) {
                result.tile = recipe.checker_tile;
                result.prefab = recipe.checker_prefab;
            } else {
                result.tile = recipe.tile;
                result.prefab = recipe.prefab;
            }
            break;
        case PaintGeneratorSourceKind::Autotile: {
            auto format{ recipe.autotile_format == PaintGeneratorAutotileFormat::Blob47 ?
                TilemapAutotileFormat::Blob47 : TilemapAutotileFormat::Subset16 };
            int index{ AutotileSlot(format, [&data](V2_int c) {
                return PaintIncludes(data, c);
            }, cell) };
            if (index >= 0 && index < static_cast<int>(recipe.autotile_tiles.size())) {
                result.tile = recipe.autotile_tiles[static_cast<std::size_t>(index)];
            }
            break;
        }
        case PaintGeneratorSourceKind::Noise:
            for (const auto& threshold : recipe.noise.thresholds) {
                if (!threshold.enabled) {
                    continue;
                }
                float value{ PaintNoise(recipe.noise, center) };
                float lo{ std::min(threshold.minimum, threshold.maximum) };
                float hi{ std::max(threshold.minimum, threshold.maximum) };
                if (value < lo || (value >= hi && !(hi >= 0.99999f && value <= 1.0f))) {
                    continue;
                }
                result.origin = threshold.origin;
                if (threshold.source_kind == PaintGeneratorSourceKind::WeightedSet) {
                    if (tile_mode) {
                        auto i{ PaintWeightedChoice(threshold.weighted_tiles, cell) };
                        if (i) {
                            result.tile = threshold.weighted_tiles[*i].source;
                        }
                    } else {
                        auto i{ PaintWeightedChoice(threshold.weighted_prefabs, cell) };
                        if (i) {
                            result.prefab = threshold.weighted_prefabs[*i].prefab;
                        }
                    }
                } else {
                    result.tile = threshold.tile;
                    result.prefab = threshold.prefab;
                }
                break;
            }
            break;
    }
    return result;
}

// Runtime-spawned entities retain their UUIDs across chunk unload and reload.
Entity RestoreStreamedTree(Scene& scene, const SerializedEntity& saved) {
    Entity entity{ scene.CreateEntity(Tag{ saved.tag },
        saved.uuid.value_or(UUID{})) };
    DeserializeEntity(saved, entity);
    for (const auto& child : saved.children) {
        Entity restored_child{ RestoreStreamedTree(scene, child) };
        if (restored_child) {
            SetParent(restored_child, entity);
        }
    }
    return entity;
}

} // namespace

bool ChunkManager::IsEnabled() const {
	return settings_.world_size.IsPositive();
}

ChunkSettings ChunkManager::GetSettings() const {
	return settings_;
}

void ChunkManager::SetSettings(ChunkSettings settings) {
	settings.render_viewports = std::isfinite(settings.render_viewports) ?
		std::clamp(settings.render_viewports, 1.0f, 32.0f) : 1.0f;
	settings.simulation_viewports = std::isfinite(settings.simulation_viewports) ?
		std::clamp(settings.simulation_viewports, 1.0f, 32.0f) : 1.5f;
	settings.render_distance = std::clamp(settings.render_distance, 0, 1024);
	settings.simulation_distance = std::clamp(settings.simulation_distance, 0, 1024);
	settings.keep_alive_distance = std::clamp(
		settings.keep_alive_distance, std::max(settings.render_distance, settings.simulation_distance), 1024
	);
	settings.max_loaded_chunks = std::clamp(settings.max_loaded_chunks, 1, 16384);
	PTGN_ASSERT(
		chunks_.empty() || settings.world_size == settings_.world_size,
		"Unload chunks before changing the world chunk size"
	);
	if (settings.save_directory != settings_.save_directory) {
		SetSaveDirectory(settings.save_directory);
	}
	settings_ = settings;
}

void ChunkManager::SetChunkSize(V2_int world_size) {
	auto settings{ settings_ };
	settings.world_size = world_size;
	SetSettings(settings);
}

void ChunkManager::Disable() {
	SetChunkSize({});
}

V2_int ChunkManager::WorldToChunk(V2_float position) const {
	V2_int size{ EffectiveSize() };
	return {
		FloorToInt(position.x / static_cast<float>(size.x)),
		FloorToInt(position.y / static_cast<float>(size.y))
	};
}

bool ChunkManager::IsStreaming() const {
	return IsEnabled() || viewport_pages_;
}

V2_int ChunkManager::EffectiveSize() const {
	return IsEnabled() ? settings_.world_size : kViewportPageSize;
}

bool ChunkManager::IsLoaded(V2_int coordinate) const {
	return !IsStreaming() || chunks_.contains(coordinate);
}

std::size_t ChunkManager::GetLoadedCount() const {
	return chunks_.size();
}

bool ChunkManager::OwnsEntity(Entity entity) const {
	return entity && OwnedCoordinate(entity).has_value();
}

bool ChunkManager::IsManagedRuntimeEntity(Entity entity) const {
    if (!entity) {
        return false;
    }

    if (OwnsEntity(entity)) {
        return true;
    }

    // Collider proxies are created only by the streaming manager. Avoid a
    // reverse linear scan through collider_proxies_ for every hierarchy entry.
    return entity.Has<impl::WorldStreamingRuntimeTag>();
}

std::optional<V2_int> ChunkManager::OwnedCoordinate(Entity entity) const {
	for (std::size_t depth{ 0 }; entity && depth < kMaxParentDepth; ++depth) {
		auto it{ owned_entities_.find(static_cast<int>(entity.Get<UUID>())) };
		if (it != owned_entities_.end()) {
			return it->second;
		}
		if (!HasParent(entity)) {
			break;
		}
		Entity parent{ GetParent(entity) };
		if (!parent || parent == entity) {
			break;
		}
		entity = parent;
	}
	return std::nullopt;
}

void ChunkManager::SetAlwaysActive(Entity entity, bool active) {
	if (!entity) {
		return;
	}
	int uuid{ static_cast<int>(entity.Get<UUID>()) };

	if (active) {
		always_active_.insert(uuid);
	} else {
		always_active_.erase(uuid);
	}
}

void ChunkManager::BeginRuntime() {
    tracking_runtime_ = true;
    pending_new_entities_.clear();
    paint_generators_registered_ = false;
}

void ChunkManager::OnEntityCreated(Entity entity) {
    if (tracking_runtime_ && !suppress_creation_tracking_ && entity) {
        pending_new_entities_.insert(static_cast<int>(entity.Get<UUID>()));
    }
}

void ChunkManager::SetEntityPolicy(Entity entity, WorldEntityPolicy policy) {
    if (!entity) {
        return;
    }
    entity.TryAdd<WorldEntityStreaming>();
    entity.Get<WorldEntityStreaming>().policy = policy;
    if (policy == WorldEntityPolicy::Global) {
        SetAlwaysActive(entity, true);
    } else {
        SetAlwaysActive(entity, false);
    }
    if (tracking_runtime_ && policy == WorldEntityPolicy::Persistent &&
        !OwnsEntity(entity)) {
        pending_new_entities_.insert(static_cast<int>(entity.Get<UUID>()));
    }
}

bool ChunkManager::IsSimulationChunk(V2_int coordinate) const {
	if (!IsStreaming()) {
		return true;
	}
	return coordinate.x >= simulation_min_.x && coordinate.x <= simulation_max_.x &&
		   coordinate.y >= simulation_min_.y && coordinate.y <= simulation_max_.y;
}

bool ChunkManager::IsSpatiallyVisible(Entity entity, BoundingAABB bounds) const {
	// No transform means this object acts as a scene controller, not spatial world content.
	// UI, cameras and tilemap containers must remain available independent of viewport.
	if (!entity || !entity.Has<Transform>() || IsUI(entity) ||
		entity.Has<impl::CameraData>() || entity.Has<impl::TilemapData>() ||
		always_active_.contains(static_cast<int>(entity.Get<UUID>()))) {
		return true;
	}
	// Script-only scene controllers should not become dormant merely because the
	// camera moves away from world origin. Physics and drawable objects are spatial.
	if (!HasDraw(entity) && !entity.Has<Collider>() && !entity.Has<RigidBody>() &&
		!entity.Has<TopDownMovement>() && !entity.Has<PlatformerMovement>() &&
		!entity.Has<impl::Interactive>()) {
		return true;
	}
	// Renderable bounds must never be replaced with the (usually smaller) collider.
	// Trees in particular use a trunk collider below their visible canopy.
	BoundingAABB entity_bounds;
	bool has_bounds{ false };
	if (HasDraw(entity)) {
		if (auto shape{ GetShape(entity) }) {
			auto draw_transform{ GetDrawTransform(entity) };
			entity_bounds = shape->Visit([draw_transform](const auto& geometry) {
				return GetBoundingAABB(geometry, draw_transform);
			});
			has_bounds = true;
		} else if (HasDraw<Sprite>(entity)) {
			if (auto size{ GetDisplaySize(entity) }) {
				V2_float position{ GetWorldPosition(entity) };
				// Full-size conservative bounds also cover bottom-aligned sprites.
				entity_bounds = { position - *size, position + *size };
				has_bounds = true;
			}
		}
	}
	if (auto* collider{ entity.TryGet<Collider>() }) {
		BoundingAABB collision_bounds{
			GetBoundingAABB(collider->shape, GetWorldTransform(entity))
		};
		if (has_bounds) {
			entity_bounds.min = Min(entity_bounds.min, collision_bounds.min);
			entity_bounds.max = Max(entity_bounds.max, collision_bounds.max);
		} else {
			entity_bounds = collision_bounds;
			has_bounds = true;
		}
	}
	if (!has_bounds) {
		V2_float position{ GetWorldPosition(entity) };
		entity_bounds = { position, position };
	}
	return entity_bounds.Overlaps(bounds);
}

bool ChunkManager::ShouldSimulate(Entity entity) const {
	if (!entity) {
		return false;
	}
	if (!view_bounds_valid_) {
		return true;
	}
	if (!IsSpatiallyVisible(entity, simulation_bounds_)) {
		return false;
	}
	auto coordinate{ OwnedCoordinate(entity) };
	return !coordinate || IsSimulationChunk(*coordinate);
}

bool ChunkManager::ShouldRender(Entity entity) const {
	if (!entity) {
		return false;
	}
	if (!view_bounds_valid_) {
		return true;
	}
	if (!IsSpatiallyVisible(entity, render_bounds_)) {
		return false;
	}
	auto owned_coordinate{ OwnedCoordinate(entity) };
	if (!owned_coordinate || !IsStreaming()) {
		return true;
	}
	// Residency plus full drawable AABB is sufficient: the owner cell can be
	// outside the view while a large sprite canopy still intersects it.
	return chunks_.contains(*owned_coordinate);
}

bool ChunkManager::ShouldRender(Entity entity, SceneCamera camera) const {
	if (!entity) {
		return false;
	}
	// Apply this camera's own viewport, not the primary camera's bounds.
	if (camera) {
		auto vertices{ camera.GetWorldVertices() };
		BoundingAABB bounds{ GetBoundingAABB(vertices) };
		V2_float center{ (bounds.min + bounds.max) * 0.5f };
		V2_float half{ (bounds.max - bounds.min) * (0.5f * settings_.render_viewports) };
		bounds = { center - half, center + half };

		if (!IsSpatiallyVisible(entity, bounds)) {
			return false;
		}
	}
	// Chunk residence is checked in the primary camera streaming grid. Cameras not
	// sharing that area can render authored content, but not unloaded generated content.
	auto coordinate{ OwnedCoordinate(entity) };
	return !coordinate || !IsStreaming() || chunks_.contains(*coordinate);
}

void ChunkManager::UpdateViewBounds(SceneCamera camera) {
	auto vertices{ camera.GetWorldVertices() };
	BoundingAABB bounds{ GetBoundingAABB(vertices) };
	V2_float center{ (bounds.min + bounds.max) * 0.5f };
	V2_float half{ (bounds.max - bounds.min) * 0.5f };
	render_bounds_ = {
		center - half * settings_.render_viewports,
		center + half * settings_.render_viewports
	};
	simulation_bounds_ = {
		center - half * settings_.simulation_viewports,
		center + half * settings_.simulation_viewports
	};
	view_bounds_valid_ = true;
}

void ChunkManager::AddEntityGenerator(std::string id, V2_float spacing, EntityFactory factory, V2_float origin) {
	PTGN_ASSERT(spacing.IsPositive() && !id.empty() && factory, "Invalid chunk entity generator");
	PTGN_ASSERT(chunks_.empty(), "Register chunk generators before the first runtime update");
	PTGN_ASSERT(
		std::ranges::none_of(entity_generators_, [&](const auto& generator) {
			return generator.id == id;
		}),
		"Chunk entity generator IDs must be unique"
	);
	entity_generators_.push_back({ std::move(id), spacing, origin, std::move(factory) });
}

void ChunkManager::AddTileGenerator(std::string id, Tilemap tilemap, TileFactory factory) {
	PTGN_ASSERT(!id.empty() && tilemap && factory, "Invalid chunk tile generator");
	PTGN_ASSERT(chunks_.empty(), "Register chunk generators before the first runtime update");
	tile_generators_.push_back({ std::move(id), tilemap.Get<UUID>(), std::move(factory) });
}

void ChunkManager::ClearGenerators() {
	PTGN_ASSERT(chunks_.empty(), "Unload all chunks before clearing generators");
	entity_generators_.clear();
	tile_generators_.clear();
}

std::string ChunkManager::TileKey(Tilemap tilemap, V2_int cell) const {
	return CellKey(std::to_string(static_cast<int>(tilemap.Get<UUID>())), cell);
}

void ChunkManager::AddManagedTile(
	Chunk& chunk, Tilemap tilemap, V2_int cell, std::optional<TilemapTile> baseline,
	std::optional<TilemapTile> restore
) {
	std::string key{ TileKey(tilemap, cell) };
	if (!chunk.tile_keys.insert(key).second) {
		return;
	}
	chunk.tiles.push_back({ tilemap, cell, std::move(key), std::move(baseline), std::move(restore) });
}

void ChunkManager::BeforeTileChanged(Tilemap tilemap, V2_int cell) {
	if (!IsStreaming() || modifying_tiles_ || !tilemap.HasScene() ||
		!tilemap.GetScene().IsRuntime()) {
		return;
	}
	V2_int coordinate{ WorldToChunk(tilemap.CellToWorld(cell)) };
	auto it{ chunks_.find(coordinate) };
	if (it == chunks_.end()) {
		return;
	}
	const TilemapTile* tile{ tilemap.FindTile(cell) };
	std::optional<TilemapTile> baseline{ tile ? std::optional<TilemapTile>{ *tile } : std::nullopt };
	AddManagedTile(it->second, tilemap, cell, baseline, baseline);
}

void ChunkManager::AfterTileChanged(Tilemap tilemap, V2_int cell) {
	if (modifying_tiles_ || !tilemap.HasScene() || !tilemap.GetScene().IsRuntime()) {
		return;
	}
	SyncTileCollider(tilemap, cell);
}

void ChunkManager::SyncTileCollider(Tilemap tilemap, V2_int cell) {
	V2_int coordinate{ WorldToChunk(tilemap.CellToWorld(cell)) };
	auto chunk_it{ chunks_.find(coordinate) };
	if (chunk_it == chunks_.end()) {
		return;
	}
	std::string key{ TileKey(tilemap, cell) };
	const TilemapTile* tile{ tilemap.FindTile(cell) };
	bool collidable{ tile && tile->collidable.value_or(false) };
	auto current{ collider_proxies_.find(key) };
	if (current != collider_proxies_.end() && (!collidable || !current->second)) {
		Entity proxy{ current->second };
		std::erase_if(chunk_it->second.colliders, [&](const TileCollider& entry) {
			if (entry.key != key) {
				return false;
			}
			owned_entities_.erase(entry.uuid);
			return true;
		});
		if (proxy) {
			proxy.Destroy();
		}
		collider_proxies_.erase(current);
		current = collider_proxies_.end();
	}
    if constexpr (kMergeTileColliders) {
        if (!rebuilding_colliders_) {
            RebuildColliders(tilemap.GetScene(), coordinate);
        }
        return;
    }
	if (!collidable) {
		return;
	}
	V2_float half{ tilemap.GetData().cell_size * 0.5f };
	V2_float position{ tilemap.CellToWorld(cell) + half };
	if (current != collider_proxies_.end() && current->second) {
		current->second.Get<Transform>().position = position;
		current->second.Get<Collider>().shape = Rect{ tilemap.GetData().cell_size };
		return;
	}
	Scene& scene{ tilemap.GetScene() };
    bool old_tracking{ suppress_creation_tracking_ };
    suppress_creation_tracking_ = true;
	Entity proxy{ scene.CreateEntity(Tag{ "Tile Collider" }) };
    proxy.Add<impl::WorldStreamingRuntimeTag>();
    suppress_creation_tracking_ = old_tracking;
	proxy.Add<Transform>(position);
	proxy.Add<Collider>(Rect{ tilemap.GetData().cell_size });
	collider_proxies_[key] = proxy;
	int uuid{ static_cast<int>(proxy.Get<UUID>()) };
	owned_entities_[uuid] = coordinate;
	chunk_it->second.colliders.push_back({ proxy, tilemap, cell, key, uuid });
}

void ChunkManager::PopulateColliders(Scene& scene, V2_int coordinate) {
    // Merged proxies are built once per loaded region, not per tile.
    if constexpr (kMergeTileColliders) {
        RebuildColliders(scene, coordinate);
        return;
    }
	// Use the Tilemap's indexed cell lookups when the requested streaming page is
	// sparse relative to the authored map. No whole-world per-page scan is needed.
	V2_int page_size{ EffectiveSize() };
	V2_float page_min{ static_cast<float>(coordinate.x) * static_cast<float>(page_size.x),
		static_cast<float>(coordinate.y) * static_cast<float>(page_size.y) };
	V2_float page_max{ page_min + V2_float{ page_size } };
	for (auto [entity, data] : scene.EntitiesWith<impl::TilemapData>()) {
		Tilemap tilemap{ entity };
		V2_int first{ tilemap.WorldToCell(page_min) };
		V2_int last{ tilemap.WorldToCell(page_max) };
		long long nx{ static_cast<long long>(last.x) - first.x + 3 };
		long long ny{ static_cast<long long>(last.y) - first.y + 3 };
		bool indexed{ nx > 0 && ny > 0 && nx <= 8192 && ny <= 8192 &&
			nx * ny < static_cast<long long>(data.tiles.size()) };

		if (indexed) {
			for (int y = first.y - 1; y <= last.y + 1; ++y) {
				for (int x = first.x - 1; x <= last.x + 1; ++x) {
					V2_int cell{ x, y };
					const TilemapTile* tile{ tilemap.FindTile(cell) };
					if (tile && tile->collidable.value_or(false) &&
						WorldToChunk(tilemap.CellToWorld(cell)) == coordinate) {
						SyncTileCollider(tilemap, cell);
					}
				}
			}
		} else {
			std::vector<V2_int> collidable_cells;
			for (const auto& tile : data.tiles) {
				if (tile.collidable.value_or(false) &&
					WorldToChunk(tilemap.CellToWorld(tile.coordinate)) == coordinate) {
					collidable_cells.push_back(tile.coordinate);
				}
			}
			for (V2_int cell : collidable_cells) {
				SyncTileCollider(tilemap, cell);
			}
		}
	}
}

bool ChunkManager::LoadChunk(Scene& scene, V2_int coordinate) {
	// Read ONLY this chunk's saved modifications, just before generation. Never
	// generate a corrupt chunk and subsequently overwrite its original save.
	if (!changes_.contains(coordinate)) {
		json delta{ json::object() };
		if (!ReadChunkDelta(coordinate, delta)) {
			if (corrupt_chunks_.insert(coordinate).second) {
				PTGN_WARN("Skipping chunk with unreadable delta: ", coordinate.x, ", ", coordinate.y);
			}
			return false;
		}
		changes_.emplace(coordinate, std::move(delta));
	}
	Chunk chunk;
	json& changes{ changes_[coordinate] };
	chunk.last_saved_delta = changes;
	if (!changes.is_object()) {
		changes = json::object();
	}
	V2_int size{ EffectiveSize() };
	V2_float origin{
		static_cast<float>(coordinate.x) * static_cast<float>(size.x),
		static_cast<float>(coordinate.y) * static_cast<float>(size.y)
	};
	V2_float far_edge{ origin + V2_float{ size } };

	for (const auto& generator : entity_generators_) {
		V2_int min{ FloorToInt((origin.x - generator.origin.x) / generator.spacing.x), FloorToInt((origin.y - generator.origin.y) / generator.spacing.y) };
		V2_int max{ CeilToInt((far_edge.x - generator.origin.x) / generator.spacing.x), CeilToInt((far_edge.y - generator.origin.y) / generator.spacing.y) };
		for (int x = min.x; x < max.x; ++x) {
			for (int y = min.y; y < max.y; ++y) {
				V2_int cell{ x, y };
				V2_float position{ generator.origin + V2_float{
                    static_cast<float>(x) * generator.spacing.x,
                    static_cast<float>(y) * generator.spacing.y } };
				if (WorldToChunk(position) != coordinate) {
					continue;
				}
				std::string key{ CellKey(generator.id, cell) };
				// Avoid constructing the tree if this cell has an explicit tombstone.
				if (HasOverride(changes, "entities", key) && changes["entities"][key].is_null()) {
					continue;
				}
                suppress_creation_tracking_ = true;
				Entity entity{ generator.factory(scene, cell) };
                suppress_creation_tracking_ = false;
				if (!entity) {
					continue;
				}
				PTGN_ASSERT(entity.HasScene() && &entity.GetScene() == &scene,
					"Chunk generator must return an entity from its scene");
				json baseline{ EntityState(entity) };
				if (HasOverride(changes, "entities", key)) {
					ApplyEntitySnapshot(scene, entity, changes["entities"][key].get<SerializedEntity>());
				}
				chunk.entities.push_back({
					std::move(key), entity, static_cast<int>(entity.Get<UUID>()), std::move(baseline)
				});
			}
		}
	}

	modifying_tiles_ = true;
	for (const auto& generator : tile_generators_) {
		Entity target{ scene.GetEntity(generator.tilemap_uuid) };
		if (!IsTilemap(target)) {
			continue;
		}
		Tilemap tilemap{ target };
		if (!tilemap) {
			continue;
		}
		V2_int min{ tilemap.WorldToCell(origin) };
		V2_int max{ tilemap.WorldToCell(far_edge) };
		// Include boundary cells for transformed maps; ownership is checked below.
		for (int x = min.x - 1; x <= max.x + 1; ++x) {
			for (int y = min.y - 1; y <= max.y + 1; ++y) {
				V2_int cell{ x, y };
				if (WorldToChunk(tilemap.CellToWorld(cell)) != coordinate || tilemap.IsExcluded(cell)) {
					continue;
				}
				const TilemapTile* authored{ tilemap.FindTile(cell) };
				if (authored) {
					continue;  // Authored tiles always have priority over generated terrain.
				}
				auto generated{ generator.factory(cell) };
				if (!generated) {
					continue;
				}
				generated->coordinate = cell;
				std::string key{ TileKey(tilemap, cell) };
				AddManagedTile(chunk, tilemap, cell, generated, std::nullopt);
				auto state{ generated };
				if (HasOverride(changes, "tiles", key)) {
					state = ReadTile(changes["tiles"][key]);
				}
				ApplyTile(tilemap, cell, state);
			}
		}
	}

	// Apply edits to authored tiles and procedural empty cells as well as generated cells.
	if (changes.contains("tiles")) {
		for (auto it = changes["tiles"].begin(); it != changes["tiles"].end(); ++it) {
			auto separator0{ it.key().find(':') };
			if (separator0 == std::string::npos) {
				continue;
			}
			Entity target;
			V2_int cell;
			try {
				target = scene.GetEntity(UUID{ std::stoi(it.key().substr(0, separator0)) });
				std::string suffix{ it.key().substr(separator0 + 1) };
				auto separator{ suffix.find(':') };
				if (separator == std::string::npos) {
					continue;
				}
				cell = { std::stoi(suffix.substr(0, separator)),
					std::stoi(suffix.substr(separator + 1)) };
			} catch (const std::exception&) {
				continue;  // Ignore malformed or out-of-range journal keys.
			}
			if (!IsTilemap(target)) {
				continue;
			}
			Tilemap tilemap{ target };
			if (WorldToChunk(tilemap.CellToWorld(cell)) != coordinate) {
				continue;
			}
			if (chunk.tile_keys.contains(it.key())) {
				continue;
			}
			const TilemapTile* authored{ tilemap.FindTile(cell) };
			std::optional<TilemapTile> baseline{ authored ? std::optional<TilemapTile>{ *authored } : std::nullopt };
			AddManagedTile(chunk, tilemap, cell, baseline, baseline);
			ApplyTile(tilemap, cell, ReadTile(it.value()));
		}
	}
	modifying_tiles_ = false;
    // UUID-preserving runtime-spawned entities are restored independently of generators.
    // Their snapshots include children and references to serializable components.
    if (changes.contains("spawned") && changes["spawned"].is_object()) {
        for (auto it = changes["spawned"].begin(); it != changes["spawned"].end(); ++it) {
            try {
                auto saved{ it.value().get<SerializedEntity>() };
                if (!saved.uuid.has_value() || scene.GetEntity(*saved.uuid)) {
                    continue;
                }
                suppress_creation_tracking_ = true;
                Entity restored{ RestoreStreamedTree(scene, saved) };
                suppress_creation_tracking_ = false;
                if (restored) {
                    int id{ static_cast<int>(restored.Get<UUID>()) };
                    chunk.spawned.push_back({ it.key(), restored, id });
                }
            } catch (const json::exception&) {
                PTGN_WARN("Ignoring malformed spawned entity in chunk ", coordinate.x, ", ", coordinate.y);
                suppress_creation_tracking_ = false;
            }
        }
    }
    for (const auto& slot : chunk.spawned) {
        owned_entities_[slot.uuid] = coordinate;
    }
	for (const auto& slot : chunk.entities) {
		owned_entities_.emplace(slot.uuid, coordinate);
	}
	chunks_.emplace(coordinate, std::move(chunk));
	PopulateColliders(scene, coordinate);
	return true;
}

void ChunkManager::SnapshotChunk(V2_int coordinate, Chunk& chunk) {
	json& changes{ changes_[coordinate] };
	if (!changes.is_object()) {
		changes = json::object();
	}
	for (const auto& slot : chunk.entities) {
		PutDelta(
			changes, "entities", slot.key, slot.entity ? EntityState(slot.entity) : json{},
			slot.baseline
		);
	}
    for (const auto& slot : chunk.spawned) {
        if (slot.entity && (!slot.entity.Has<WorldEntityStreaming>() ||
            slot.entity.Get<WorldEntityStreaming>().policy == WorldEntityPolicy::Persistent)) {
            changes["spawned"][slot.key] = json{ SerializeEntity(slot.entity) };
        } else if (changes.contains("spawned")) {
            changes["spawned"].erase(slot.key);
        }
    }
	for (const auto& slot : chunk.tiles) {
		if (!slot.tilemap) {
			continue;
		}
		const TilemapTile* current{ slot.tilemap.FindTile(slot.cell) };
		std::optional<TilemapTile> state{ current ? std::optional<TilemapTile>{ *current } : std::nullopt };
		PutDelta(changes, "tiles", slot.key, TileState(state), TileState(slot.baseline));
	}
	if (EmptyDelta(changes)) {
		changes_.erase(coordinate);
	}
}

void ChunkManager::UnloadChunk(V2_int coordinate, Scene& scene) {
	auto it{ chunks_.find(coordinate) };
	if (it == chunks_.end()) {
		return;
	}
	SnapshotChunk(coordinate, it->second);
	// Commit before destroying live entities/tiles. Unchanged chunks remove any
	// obsolete delta file, so clean worlds do not consume persistent storage.
	if (disk_writes_enabled_ && settings_.save_mode != ChunkSaveMode::Manual &&
		!save_directory_.empty()) {
		auto delta{ changes_.find(coordinate) };
		json empty{ json::object() };
		const json& state{ delta != changes_.end() ? delta->second : empty };
		if (state == it->second.last_saved_delta || WriteChunkDelta(coordinate, state)) {
			if (delta != changes_.end()) {
				changes_.erase(delta);
			}
		} else {
			PTGN_WARN("Unable to persist chunk ", coordinate.x, ", ", coordinate.y);
		}
	}
	modifying_tiles_ = true;
	for (const auto& slot : it->second.tiles) {
		if (slot.tilemap) {
			ApplyTile(slot.tilemap, slot.cell, slot.restore);
		}
	}
	modifying_tiles_ = false;
    ClearColliders(it->second);
	for (auto& slot : it->second.entities) {
		owned_entities_.erase(slot.uuid);
		if (slot.entity) {
			slot.entity.Destroy();
		}
	}
    for (auto& slot : it->second.spawned) {
        owned_entities_.erase(slot.uuid);
        if (slot.entity) {
            slot.entity.Destroy();
        }
    }
	chunks_.erase(it);
	(void)scene;
}

void ChunkManager::UnloadAll(Scene& scene) {
    AdoptNewEntities(scene);
    MigrateEntities(scene);
	std::vector<V2_int> coordinates;
	coordinates.reserve(chunks_.size());
	for (const auto& [coordinate, _] : chunks_) {
		coordinates.push_back(coordinate);
	}
	for (V2_int coordinate : coordinates) {
		UnloadChunk(coordinate, scene);
	}
	viewport_pages_ = false;
	view_bounds_valid_ = false;
	always_active_.clear();
    pending_new_entities_.clear();
    tracking_runtime_ = false;
    paint_generators_registered_ = false;
	scene.Refresh();
}

void ChunkManager::Update(Scene& scene) {
	if (!scene.IsRuntime()) {
		return;
	}
    if (!paint_generators_registered_) {
        RegisterPaintGenerators(scene);
    }
	SceneCamera camera{ scene.GetCamera() };
	UpdateViewBounds(camera);

	// Explicit chunks and internal streaming pages share the same finite scheduler.
	// UI-only games create no streaming pages at all.
	viewport_pages_ = !IsEnabled() &&
		(!entity_generators_.empty() || !tile_generators_.empty());
	if (!IsEnabled() && !viewport_pages_) {
		for (auto&& entry : scene.EntitiesWith<impl::TilemapData>()) {
            (void)entry;
			viewport_pages_ = true;
			break;
		}
	}
	if (!PrepareStorage()) {
		return;
	}
	if (disk_writes_enabled_ && settings_.save_mode == ChunkSaveMode::Automatic &&
		!save_directory_.empty() && !chunks_.empty()) {
		auto now{ std::chrono::steady_clock::now() };
		if (now - last_auto_save_ >= settings_.autosave_interval) {
			if (!SaveConfigured()) {
				PTGN_WARN("Periodic chunk autosave failed");
			}
			last_auto_save_ = now;
		}
	}
	if (!IsStreaming()) {
		return;
	}

	V2_int viewport_min{ WorldToChunk(render_bounds_.min) };
	V2_int viewport_max{ WorldToChunk(render_bounds_.max) };
	V2_int sim_view_min{ WorldToChunk(simulation_bounds_.min) };
	V2_int sim_view_max{ WorldToChunk(simulation_bounds_.max) };
	int simulation{ IsEnabled() ? settings_.simulation_distance : 0 };
	int render{ IsEnabled() ? settings_.render_distance : 0 };
	simulation_min_ = sim_view_min - V2_int{ simulation, simulation };
	simulation_max_ = sim_view_max + V2_int{ simulation, simulation };
	render_min_ = viewport_min - V2_int{ render, render };
	render_max_ = viewport_max + V2_int{ render, render };
	V2_int wanted_min{ Min(simulation_min_, render_min_) };
	V2_int wanted_max{ Max(simulation_max_, render_max_) };
	V2_float view_center{ (render_bounds_.min + render_bounds_.max) * 0.5f };
	V2_int focus{ WorldToChunk(view_center) };
	// Visit nearest cells first, without allocating a viewport-sized rectangle. This is
	// bounded by max_loaded_chunks even for tiny chunk sizes in a huge camera viewport.
	struct Candidate {
		V2_int coordinate;
		long long distance;
	};
	auto priority = [](const Candidate& a, const Candidate& b) {
		if (a.distance != b.distance) {
			return a.distance > b.distance;
		}
		return a.coordinate.x != b.coordinate.x ? a.coordinate.x > b.coordinate.x :
			a.coordinate.y > b.coordinate.y;
	};
	std::priority_queue<Candidate, std::vector<Candidate>, decltype(priority)> pending{ priority };
	std::unordered_set<V2_int> visited;
	auto enqueue = [&](V2_int coordinate) {
		if (coordinate.x < wanted_min.x || coordinate.x > wanted_max.x ||
			coordinate.y < wanted_min.y || coordinate.y > wanted_max.y ||
			!visited.insert(coordinate).second) {
			return;
		}
		long long dx{ static_cast<long long>(coordinate.x) - focus.x };
		long long dy{ static_cast<long long>(coordinate.y) - focus.y };
		pending.push({ coordinate, dx * dx + dy * dy });
	};
	enqueue({ std::clamp(focus.x, wanted_min.x, wanted_max.x),
		std::clamp(focus.y, wanted_min.y, wanted_max.y) });
	std::vector<V2_int> wanted;
	wanted.reserve(static_cast<std::size_t>(settings_.max_loaded_chunks));
	while (!pending.empty() && wanted.size() < static_cast<std::size_t>(settings_.max_loaded_chunks)) {
		V2_int coordinate{ pending.top().coordinate };
		pending.pop();
		wanted.push_back(coordinate);
		if (coordinate.x > wanted_min.x) {
			enqueue({ coordinate.x - 1, coordinate.y });
		}
		if (coordinate.x < wanted_max.x) {
			enqueue({ coordinate.x + 1, coordinate.y });
		}
		if (coordinate.y > wanted_min.y) {
			enqueue({ coordinate.x, coordinate.y - 1 });
		}
		if (coordinate.y < wanted_max.y) {
			enqueue({ coordinate.x, coordinate.y + 1 });
		}
	}
	std::unordered_set<V2_int> desired{ wanted.begin(), wanted.end() };
	// Hysteresis: retain already generated chunks inside the larger keep-alive bounds
	// when there is spare capacity, without generating new chunks outside the view.
	int keep_alive{ IsEnabled() ? settings_.keep_alive_distance : 1 };
	V2_int keep_min{ wanted_min - V2_int{ keep_alive, keep_alive } };
	V2_int keep_max{ wanted_max + V2_int{ keep_alive, keep_alive } };
	for (const auto& [coordinate, _] : chunks_) {
		if (desired.size() >= static_cast<std::size_t>(settings_.max_loaded_chunks)) {
			break;
		}
		if (coordinate.x >= keep_min.x && coordinate.x <= keep_max.x &&
			coordinate.y >= keep_min.y && coordinate.y <= keep_max.y) {
			desired.insert(coordinate);
		}
	}
	std::vector<V2_int> to_unload;
	for (const auto& [coordinate, _] : chunks_) {
		if (!desired.contains(coordinate)) {
			to_unload.push_back(coordinate);
		}
	}
    // Existing resident entities must migrate before their old pages are evicted.
    // Adopt freshly spawned entities after loading desired pages, so new entities
    // created during OnEnter are not retired before their first resident page loads.
    MigrateEntities(scene);
	// Incremental, main-thread streaming (not asynchronous). Keep the amount of
	// generation and file I/O bounded on individual frames.
	constexpr int kMaxLoadsPerFrame{ 1 };
	constexpr int kMaxUnloadsPerFrame{ 1 };
	bool changed{ false };
	int unloaded{};
	for (V2_int coordinate : to_unload) {
		if (unloaded >= kMaxUnloadsPerFrame) {
			break;
		}
		UnloadChunk(coordinate, scene);
		changed = true;
		++unloaded;
	}
	int loaded{};
	for (V2_int coordinate : wanted) {
		if (loaded >= kMaxLoadsPerFrame) {
			break;
		}
		if (!chunks_.contains(coordinate) && !corrupt_chunks_.contains(coordinate)) {
			if (LoadChunk(scene, coordinate)) {
				changed = true;
				++loaded;
			}
		}
	}
    // Newly loaded pages may have had newly created gameplay entities in OnEnter.
    AdoptNewEntities(scene);
	if (changed) {
		scene.Refresh();
	}
}

void ChunkManager::ClearColliders(Chunk& chunk) {
    for (const auto& collider : chunk.colliders) {
        collider_proxies_.erase(collider.key);
        owned_entities_.erase(collider.uuid);
        if (collider.entity) {
            Entity entity{ collider.entity };
            if (auto* shape{ entity.TryGet<Collider>() }) {
                shape->mode = CollisionMode::None;
            }
            entity.Destroy();
        }
    }
    chunk.colliders.clear();
}

void ChunkManager::RebuildColliders(Scene& scene, V2_int coordinate) {
    auto chunk_it{ chunks_.find(coordinate) };
    if (chunk_it == chunks_.end() || rebuilding_colliders_) {
        return;
    }
    rebuilding_colliders_ = true;
    ClearColliders(chunk_it->second);
    V2_int page_size{ EffectiveSize() };
    V2_float min_world{ static_cast<float>(coordinate.x) * static_cast<float>(page_size.x),
        static_cast<float>(coordinate.y) * static_cast<float>(page_size.y) };
    V2_float max_world{ min_world + V2_float{ page_size } };

    for (auto [entity, data] : scene.EntitiesWith<impl::TilemapData>()) {
        Tilemap map{ entity };
        V2_int min_cell{ map.WorldToCell(min_world) - V2_int{ 1, 1 } };
        V2_int max_cell{ map.WorldToCell(max_world) + V2_int{ 1, 1 } };
        long long count{ (static_cast<long long>(max_cell.x) - min_cell.x + 1) *
            (static_cast<long long>(max_cell.y) - min_cell.y + 1) };
        std::unordered_set<V2_int> cells;
        if (count > 0 && count < static_cast<long long>(data.tiles.size())) {
            for (int y = min_cell.y; y <= max_cell.y; ++y) {
                for (int x = min_cell.x; x <= max_cell.x; ++x) {
                    V2_int cell{ x, y };
                    auto* tile{ map.FindTile(cell) };
                    if (tile && tile->collidable.value_or(false) &&
                        WorldToChunk(map.CellToWorld(cell)) == coordinate) {
                        cells.insert(cell);
                    }
                }
            }
        } else {
            for (const auto& tile : data.tiles) {
                if (tile.collidable.value_or(false) &&
                    WorldToChunk(map.CellToWorld(tile.coordinate)) == coordinate) {
                    cells.insert(tile.coordinate);
                }
            }
        }
        // Deterministic greedy rectangular merging. Never cross a page or Tilemap boundary.
        while (!cells.empty()) {
            auto first{ std::ranges::min_element(cells, [](V2_int a, V2_int b) {
                return a.y == b.y ? a.x < b.x : a.y < b.y;
            }) };
            V2_int start{ *first };
            int width{ 1 };
            while (cells.contains(start + V2_int{ width, 0 })) {
                ++width;
            }
            int height{ 1 };
            for (;;) {
                bool row_complete{ true };
                for (int x = 0; x < width; ++x) {
                    if (!cells.contains(start + V2_int{ x, height })) {
                        row_complete = false;
                        break;
                    }
                }
                if (!row_complete) {
                    break;
                }
                ++height;
            }
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    cells.erase(start + V2_int{ x, y });
                }
            }
            V2_float size{ data.cell_size * V2_float{ static_cast<float>(width),
                static_cast<float>(height) } };
            V2_float center{ map.CellToWorld(start) + size * 0.5f };
            bool old_tracking{ suppress_creation_tracking_ };
            suppress_creation_tracking_ = true;
            Entity proxy{ scene.CreateEntity(Tag{ "Tile Collider" }) };
    proxy.Add<impl::WorldStreamingRuntimeTag>();
            suppress_creation_tracking_ = old_tracking;
            proxy.Add<Transform>(center);
            proxy.Add<Collider>(Rect{ size });
            int uuid{ static_cast<int>(proxy.Get<UUID>()) };
            std::string key{ TileKey(map, start) + "/merged/" +
                std::to_string(width) + "x" + std::to_string(height) };
            owned_entities_[uuid] = coordinate;
            collider_proxies_[key] = proxy;
            chunk_it->second.colliders.push_back({ proxy, map, start, std::move(key), uuid });
        }
    }
    rebuilding_colliders_ = false;
}

bool ChunkManager::EnsureDeltaLoaded(V2_int coordinate) {
    if (changes_.contains(coordinate)) {
        return true;
    }
    if (auto resident{ chunks_.find(coordinate) }; resident != chunks_.end()) {
        SnapshotChunk(coordinate, resident->second);
        if (!changes_.contains(coordinate)) {
            changes_[coordinate] = json::object();
        }
        return true;
    }
    json delta{ json::object() };
    if (!ReadChunkDelta(coordinate, delta)) {
        corrupt_chunks_.insert(coordinate);
        return false;
    }
    changes_.emplace(coordinate, std::move(delta));
    return true;
}

void ChunkManager::AdoptNewEntities(Scene& scene) {
    if (!IsStreaming() || pending_new_entities_.empty()) {
        return;
    }
    // Recently spawned entities may not be visible to ECS views until Refresh().
    scene.Refresh();
    auto pending{ std::move(pending_new_entities_) };
    pending_new_entities_.clear();
    for (int id : pending) {
        Entity entity{ scene.GetEntity(UUID{ id }) };
        if (!entity || !entity.Has<Transform>() || IsTilemap(entity) ||
            IsPaintGenerator(entity) || OwnedCoordinate(entity).has_value()) {
            continue;
        }
        // A spawned prefab's children follow the root, not separate owner records.
        if (HasParent(entity)) {
            continue;
        }
        auto* streaming{ entity.TryGet<WorldEntityStreaming>() };
        WorldEntityPolicy policy{ streaming ? streaming->policy : WorldEntityPolicy::Persistent };
        if (policy == WorldEntityPolicy::Global) {
            SetAlwaysActive(entity, true);
            continue;
        }
        V2_int coordinate{ WorldToChunk(GetWorldPosition(entity)) };
        std::string key{ std::to_string(id) };
        if (auto resident{ chunks_.find(coordinate) }; resident != chunks_.end()) {
            resident->second.spawned.push_back({ key, entity, id });
            owned_entities_[id] = coordinate;
        } else if (policy == WorldEntityPolicy::Persistent && EnsureDeltaLoaded(coordinate)) {
            changes_[coordinate]["spawned"][key] = json{ SerializeEntity(entity) };
            entity.Destroy();
        } else if (policy == WorldEntityPolicy::Transient) {
            entity.Destroy();
        } else {
            // Never discard an entity on a failed persistent storage read.
            SetAlwaysActive(entity, true);
            PTGN_WARN("Retaining world entity after failing to read destination chunk");
        }
    }
}

void ChunkManager::MigrateEntities(Scene& scene) {
    if (!IsStreaming()) {
        return;
    }
    // Snapshot source/destination changes before modifying ownership so a moving object
    // can never simultaneously appear in the generator's origin and destination.
    for (auto& [source, chunk] : chunks_) {
        auto transfer = [&](Entity entity, int id, const std::string& key,
                            bool from_generator, const json& baseline) {
            if (!entity || !entity.Has<Transform>()) {
                return false;
            }
            auto* policy_component{ entity.TryGet<WorldEntityStreaming>() };
            WorldEntityPolicy policy{ policy_component ? policy_component->policy :
                WorldEntityPolicy::Persistent };
            if (policy == WorldEntityPolicy::Global) {
                if (from_generator) {
                    PutDelta(changes_[source], "entities", key, json{}, baseline);
                } else if (changes_[source].contains("spawned")) {
                    changes_[source]["spawned"].erase(key);
                }
                owned_entities_.erase(id);
                SetAlwaysActive(entity, true);
                return true;
            }
            V2_int destination{ WorldToChunk(GetWorldPosition(entity)) };
            if (destination == source) {
                return false;
            }
            if (!EnsureDeltaLoaded(destination)) {
                // Keep the object resident until the destination is writable.
                return false;
            }
            if (from_generator) {
                PutDelta(changes_[source], "entities", key, json{}, baseline);
            } else if (changes_[source].contains("spawned")) {
                changes_[source]["spawned"].erase(key);
            }
            if (policy == WorldEntityPolicy::Transient) {
                owned_entities_.erase(id);
                entity.Destroy();
                return true;
            }
            std::string spawn_key{ std::to_string(id) };
            changes_[destination]["spawned"][spawn_key] = json{ SerializeEntity(entity) };
            auto destination_it{ chunks_.find(destination) };
            if (destination_it != chunks_.end()) {
                destination_it->second.spawned.push_back({ spawn_key, entity, id });
                owned_entities_[id] = destination;
            } else {
                owned_entities_.erase(id);
                entity.Destroy();
            }
            return true;
        };
        for (auto it = chunk.entities.begin(); it != chunk.entities.end();) {
            if (transfer(it->entity, it->uuid, it->key, true, it->baseline)) {
                it = chunk.entities.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = chunk.spawned.begin(); it != chunk.spawned.end();) {
            if (transfer(it->entity, it->uuid, it->key, false, json{})) {
                it = chunk.spawned.erase(it);
            } else {
                ++it;
            }
        }
        if (EmptyDelta(changes_[source])) {
            changes_.erase(source);
        }
    }
    (void)scene;
}

void ChunkManager::RegisterPaintGenerators(Scene& scene) {
    if (paint_generators_registered_) {
        return;
    }
    PTGN_ASSERT(chunks_.empty(), "Runtime generators must be registered before streaming");
    paint_generators_registered_ = true;
    for (auto [entity, authored] : scene.EntitiesWith<impl::PaintGeneratorData>()) {
        // Custom world generators are registered by the owning Scene with its
        // PaintGenerator as the editable source. Do not register an extra empty
        // prefab generator or repeat the sampling loop for these entities.
        if (entity.Has<WorldGeneratorSource>()) {
            continue;
        }

        if (!authored.enabled || !authored.stream_at_runtime.value_or(true) ||
            !authored.grid_size.IsPositive()) {
            continue;
        }
        PaintGenerator generator{ entity };
        impl::PaintGeneratorData data{ authored };
        V2_float origin{ GetWorldPosition(entity) + data.grid_offset };
        std::string id{ "paint:" + std::to_string(static_cast<int>(entity.Get<UUID>())) };
        Tilemap target{ generator.GetTargetTilemap() };
        if (target) {
            // A Tilemap cell is the atomic writable unit. Sample the authoring grid at
            // that cell; when the grids differ, don't emit several overlapping tiles.
            AddTileGenerator(id, target, [data, origin, target](V2_int tile_cell)
                -> std::optional<TilemapTile> {
                if (!target) {
                    return std::nullopt;
                }
                V2_float world{ target.CellToWorld(tile_cell) };
                V2_float relative{ world - origin };
                V2_int cell{ FloorToInt(relative.x / data.grid_size.x),
                    FloorToInt(relative.y / data.grid_size.y) };
                V2_float anchor{ origin + V2_float{
                    static_cast<float>(cell.x) * data.grid_size.x,
                    static_cast<float>(cell.y) * data.grid_size.y } };
                V2_float epsilon{ data.grid_size * 0.001f };
                // Only tile-sized placements aligned to the generator grid are emitted.
                if (std::abs(anchor.x - world.x) > epsilon.x ||
                    std::abs(anchor.y - world.y) > epsilon.y ||
                    !PaintIncludes(data, cell)) {
                    return std::nullopt;
                }
                if (data.recipe.avoid_exclusion_mask && target.IsExcluded(tile_cell)) {
                    return std::nullopt;
                }
                auto source{ SelectPaintSource(data, cell,
                    anchor + data.grid_size * 0.5f, true) };
                if (!source.tile) {
                    return std::nullopt;
                }
                const auto& tile{ *source.tile };
                return TilemapTile{
                    .coordinate = tile_cell,
                    .texture = tile.texture,
                    .texture_coordinates = tile.texture_coordinates,
                    .pixel_size = tile.pixel_size,
                    .origin = tile.origin,
                    .offset = {},
                    .tint = color::White,
                    .collidable = std::nullopt,
                    .terrain_ruleset_id = std::nullopt
                };
            });
        } else {
            AddEntityGenerator(id, data.grid_size,
                [data, origin](Scene& world, V2_int cell) -> Entity {
                    if (!PaintIncludes(data, cell)) {
                        return {};
                    }
                    V2_float anchor{ origin + V2_float{
                        static_cast<float>(cell.x) * data.grid_size.x,
                        static_cast<float>(cell.y) * data.grid_size.y } };
                    auto source{ SelectPaintSource(data, cell,
                        anchor + data.grid_size * 0.5f, false) };
                    if (!source.prefab.has_value()) {
                        return {};
                    }
                    auto& assets{ world.ctx().asset };
                    if (!assets.EnsurePrefabResident(*source.prefab)) {
                        return {};
                    }
                    auto prefab{ impl::AssetAccessor{ assets }.Get<Prefab>(*source.prefab) };
                    Entity created{ InstantiatePrefab(
                        world, prefab.get(), PrefabInstantiationMode::Baked) };
                    if (!created) {
                        return {};
                    }
                    Transform transform{ GetWorldTransform(created) };
                    transform.position = anchor + GetOffset(source.origin, data.grid_size);
                    SetWorldTransform(created, transform);
                    if (data.recipe.random_rotation) {
                        SetRotation(created, Degrees{
                            data.recipe.rotation_min + (data.recipe.rotation_max -
                                data.recipe.rotation_min) * PaintRandom(cell) });
                    }
                    if (data.recipe.random_scale) {
                        SetScale(created, data.recipe.scale_min +
                            (data.recipe.scale_max - data.recipe.scale_min) *
                            PaintRandom(V2_int{ cell.y, cell.x }));
                    }
                    return created;
                }, origin);
        }
    }
}

void ChunkManager::SetSaveDirectory(path directory) {
	PTGN_ASSERT(chunks_.empty(), "Configure chunk storage before starting world streaming");
	settings_.save_directory = directory.generic_string();
	save_directory_ = StoragePath(std::move(directory));
	legacy_save_path_.clear();
	storage_prepared_ = false;
	last_auto_save_ = std::chrono::steady_clock::now();
	corrupt_chunks_.clear();
}

const path& ChunkManager::GetSaveDirectory() const {
	return save_directory_;
}

void ChunkManager::SetSavePath(path filepath) {
	if (filepath.extension() == ".json") {
		auto legacy{ StoragePath(filepath) };
		SetSaveDirectory(filepath.parent_path() / filepath.stem());
		legacy_save_path_ = std::move(legacy);
	} else {
		SetSaveDirectory(std::move(filepath));
	}
}

path ChunkManager::ChunkFile(V2_int coordinate) const {
	// Implicit viewport pages are not compatible with a user's configured chunk
	// grid. Keep their saves separate so toggling chunks never misreads deltas.
	path directory{ IsEnabled() ? save_directory_ :
		(save_directory_ / "viewport_512") };
	return directory /
		(std::to_string(coordinate.x) + "_" + std::to_string(coordinate.y) + ".json");
}

bool ChunkManager::ReadChunkDelta(V2_int coordinate, json& delta) const {
	if (save_directory_.empty()) {
		return true;
	}
	path filepath{ ChunkFile(coordinate) };
	std::error_code error;
	if (!fs::exists(filepath, error)) {
		if (error) {
			return false;
		}
		// A Windows crash between backup and replacement can leave only a .bak.
		auto backup{ path{ filepath.string() + ".bak" } };
		if (!fs::exists(backup, error)) {
			return !error;
		}
		filepath = std::move(backup);
	}
	std::ifstream input{ filepath, std::ios::binary };
	if (!input) {
		return false;
	}
	try {
		json saved{ json::parse(input) };
		if (!saved.is_object() || saved.value("version", 0) != 1 ||
			!saved.contains("coordinate") ||
			saved["coordinate"] != json::array({ coordinate.x, coordinate.y }) ||
			!saved.contains("changes") || !saved["changes"].is_object()) {
			return false;
		}
		delta = std::move(saved["changes"]);
		return true;
	} catch (const json::exception&) {
		return false;
	}
}

bool ChunkManager::WriteChunkDelta(V2_int coordinate, const json& delta) {
	if (!disk_writes_enabled_ || save_directory_.empty()) {
		return false;
	}
	path filepath{ ChunkFile(coordinate) };
	if (EmptyDelta(delta)) {
		std::error_code error;
		bool exists{ fs::exists(filepath, error) };
		if (error) {
			return false;
		}
		bool backup_exists{ fs::exists(
			path{ filepath.string() + ".bak" }, error
		) };
		if (error) {
			return false;
		}
		if (!exists && !backup_exists) {
			return true;  // A pristine page has no file.
		}
		fs::remove(filepath, error);
		if (error) {
			return false;
		}
		fs::remove(path{ filepath.string() + ".bak" }, error);
		if (error) {
			return false;
		}
	} else if (!CommitJson(filepath, {
		{ "version", 1 },
		{ "coordinate", json::array({ coordinate.x, coordinate.y }) },
		{ "changes", delta }
	})) {
		return false;
	}
	return true;
}

bool ChunkManager::PrepareStorage() {
	if (save_directory_.empty()) {
		return true;
	}
	if (storage_prepared_) {
		return true;
	}
	// Import a legacy monolithic journal, if configured.
	// Migrate all chunks before deleting the old journal. New per-chunk files win if
	// migration was interrupted and restarted partway through.
	if (disk_writes_enabled_ && !legacy_save_path_.empty()) {
		std::error_code error;
		if (fs::exists(legacy_save_path_, error)) {
			if (error) {
				return false;
			}
			if (!LoadFromFile(legacy_save_path_)) {
				PTGN_WARN("Cannot read legacy chunk journal: ", legacy_save_path_.string());
				return false;
			}
			for (const auto& [coordinate, delta] : changes_) {
				if (fs::exists(ChunkFile(coordinate), error)) {
					continue;
				}
				if (error || !WriteChunkDelta(coordinate, delta)) {
					PTGN_WARN("Cannot migrate old chunk journal to per-chunk files");
					return false;
				}
			}
			fs::remove(legacy_save_path_, error);
			if (error) {
				return false;
			}
			// Start normal lazy loading; don't retain all migrated changes in memory.
			changes_.clear();
		} else if (error) {
			return false;
		}
	}
	storage_prepared_ = true;
	return true;
}

json ChunkManager::SerializeChanges() {
	for (auto& [coordinate, chunk] : chunks_) {
		SnapshotChunk(coordinate, chunk);
	}
	// This explicit export API produces the old whole-world journal. Regular
	// streaming never scans other chunks; this may intentionally be expensive.
	std::map<std::pair<int, int>, json> entries;
	if (storage_prepared_ && !save_directory_.empty()) {
		std::error_code error;
		fs::recursive_directory_iterator it{
			save_directory_, fs::directory_options::skip_permission_denied, error
		};
		if (!error) {
			for (const auto& file : it) {
				if (!file.is_regular_file() || file.path().extension() != ".json") {
					continue;
				}
				std::ifstream input{ file.path() };
				try {
					json entry{ json::parse(input) };
					if (!entry.is_object() || entry.value("version", 0) != 1 ||
						!entry.contains("coordinate") || !entry["coordinate"].is_array() ||
						entry["coordinate"].size() != 2 || !entry.contains("changes") ||
						!entry["changes"].is_object()) {
						continue;
					}
					V2_int coordinate{ entry["coordinate"][0].get<int>(),
						entry["coordinate"][1].get<int>() };
					if (file.path() == ChunkFile(coordinate)) {
						entries[{ coordinate.x, coordinate.y }] = std::move(entry["changes"]);
					}
				} catch (const json::exception&) {
					PTGN_WARN("Cannot export unreadable chunk file: ", file.path().string());
				}
			}
		}
	}
	for (const auto& [coordinate, delta] : changes_) {
		entries[{ coordinate.x, coordinate.y }] = delta;
	}
	json result = { { "version", 1 }, { "chunks", json::array() } };
	for (const auto& [coordinate, delta] : entries) {
		if (!EmptyDelta(delta)) {
			result["chunks"].push_back({
				{ "coordinate", json::array({ coordinate.first, coordinate.second }) },
				{ "changes", delta }
			});
		}
	}
	return result;
}

bool ChunkManager::DeserializeChanges(const json& data) {
	if (!chunks_.empty() || !data.is_object() || data.value("version", 0) != 1 ||
		!data.contains("chunks") || !data["chunks"].is_array()) {
		return false;
	}
	std::unordered_map<V2_int, json> next;
	try {
		for (const auto& entry : data.at("chunks")) {
			const auto& position{ entry.at("coordinate") };
			if (!position.is_array() || position.size() != 2) {
				return false;
			}
			V2_int coordinate{ position[0].get<int>(), position[1].get<int>() };
			json delta{ entry.at("changes") };
			if (!delta.is_object()) {
				return false;
			}
			if (!EmptyDelta(delta)) {
				next.emplace(coordinate, std::move(delta));
			}
		}
	} catch (const json::exception&) {
		return false;
	}
	changes_ = std::move(next);
	return true;
}

bool ChunkManager::LoadFromFile(const path& filepath) {
	std::ifstream input{ filepath };
	if (!input) {
		return false;
	}
	try {
		json data{ json::parse(input) };
		return DeserializeChanges(data);
	} catch (const json::exception&) {
		return false;
	}
}

bool ChunkManager::SaveToFile(const path& filepath) {
	if (!disk_writes_enabled_ || filepath.empty() || !PrepareStorage()) {
		return false;
	}
	// Optional explicit export of a whole-world journal; not part of streaming.
	return CommitJson(StoragePath(filepath), SerializeChanges());
}

void ChunkManager::SetDiskWritesEnabled(bool enabled) {
	disk_writes_enabled_ = enabled;
}

bool ChunkManager::AreDiskWritesEnabled() const {
	return disk_writes_enabled_;
}

bool ChunkManager::SaveConfigured() {
	if (!disk_writes_enabled_ || save_directory_.empty() || !PrepareStorage()) {
		return false;
	}
	for (auto& [coordinate, chunk] : chunks_) {
		SnapshotChunk(coordinate, chunk);
	}
	bool saved{ true };
	for (auto it = changes_.begin(); it != changes_.end();) {
		auto resident{ chunks_.find(it->first) };
		if (resident != chunks_.end() && it->second == resident->second.last_saved_delta) {
			++it;
			continue;
		}
		if (!WriteChunkDelta(it->first, it->second)) {
			saved = false;
			++it;
		} else if (resident == chunks_.end()) {
			it = changes_.erase(it);
		} else {
			resident->second.last_saved_delta = it->second;
			++it;
		}
	}
	// A previously modified resident chunk can revert to its baseline. In that
	// case SnapshotChunk removes its entry entirely, so delete the old disk file.
	for (auto& [coordinate, chunk] : chunks_) {
		if (!changes_.contains(coordinate) && !EmptyDelta(chunk.last_saved_delta)) {
			json empty{ json::object() };
			if (WriteChunkDelta(coordinate, empty)) {
				chunk.last_saved_delta = std::move(empty);
			} else {
				saved = false;
			}
		}
	}
	return saved;
}

} // namespace ptgn
