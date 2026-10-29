#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/world/tilemap.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class SceneCamera;
struct Camera;

/// Spatial world entities created during play are persistent by default. This component
/// overrides that default; authored scene entities remain scene owned unless explicitly tracked.
enum class WorldEntityPolicy : std::uint8_t {
	Persistent,
	Transient,
	Global
};
PTGN_REFLECT_ENUM(WorldEntityPolicy);

struct WorldEntityStreaming {
	WorldEntityPolicy policy{ WorldEntityPolicy::Persistent };
	PTGN_REFLECT(WorldEntityStreaming, policy)
};

/// Associates an authored Paint Generator with a custom, deterministic runtime
/// world generator registered by its scene. It is a generator definition, not
/// a parent for generated instances (which are streamed independently).
struct WorldGeneratorSource {
	std::string id{};
	// Optional visual metadata for editor previews of custom runtime generators.
	// Empty texture means this source supplies no automatic sprite highlight.
	TextureKey preview_texture{};
	V2_float preview_size{};
	V2_float preview_origin{};
	std::uint32_t preview_jitter_salt{};
	std::uint32_t preview_density_salt{};
	// Optional centered rectangular exclusion (e.g. an authored building clearing).
	V2_float preview_exclusion_half_size{};
	PTGN_REFLECT(WorldGeneratorSource, id, preview_texture, preview_size,
		preview_origin, preview_jitter_salt, preview_density_salt,
		preview_exclusion_half_size)
};

namespace impl {
/// Runtime only marker. Never save generated instances into the authored scene.
struct WorldStreamingRuntimeTag {};
} // namespace impl

/// Manual: explicit SaveConfigured() only; in memory changes remain until saved.
/// OnUnload: save when pages leave memory and on normal scene exit.
/// Automatic: on unload plus periodic resident page autosaves (default).
enum class ChunkSaveMode : std::uint8_t {
    Manual,
    OnUnload,
    Automatic
};
PTGN_REFLECT_ENUM(ChunkSaveMode);

/// Viewport culling is always available, whether or not explicit chunks are enabled.
/// A non positive world_size disables explicit chunks. Procedural content can still use
/// bounded internal streaming pages: there is no infinite default chunk.
/// Chunk distances are additional chunk counts around the camera bounds.
struct ChunkSettings {
	V2_int world_size{};
	/// 1.0 = exact viewport, 1.5 = 50% larger in each dimension.
	float render_viewports{ 1.0f };
	float simulation_viewports{ 1.5f };
	int render_distance{ 1 };
	int simulation_distance{ 1 };
	int keep_alive_distance{ 2 };
	int max_loaded_chunks{ 128 };
	/// Empty means in memory deltas only; set a directory to enable automatic disk saves.
	std::string save_directory{};
    ChunkSaveMode save_mode{ ChunkSaveMode::Automatic };
    seconds autosave_interval{ 15s };

	PTGN_REFLECT(
		ChunkSettings, world_size, render_viewports, simulation_viewports, render_distance,
		simulation_distance, keep_alive_distance, max_loaded_chunks, save_directory, save_mode,
        autosave_interval
	)
};

/// Tracks ephemeral procedural content and retains ONLY differences from the deterministic
/// generator output. Authored scene entities/tiles are never streamed or destroyed by default.
class ChunkManager {
public:
	using EntityFactory = std::function<Entity(Scene&, V2_int)>;
	using TileFactory	= std::function<std::optional<TilemapTile>(V2_int)>;

	ChunkManager()									 = default;
	ChunkManager(const ChunkManager&)				 = delete;
	ChunkManager& operator=(const ChunkManager&) = delete;
	ChunkManager(ChunkManager&&) noexcept			 = default;
	ChunkManager& operator=(ChunkManager&&) noexcept = default;
	~ChunkManager()									 = default;

	[[nodiscard]] bool IsEnabled() const;
	[[nodiscard]] ChunkSettings GetSettings() const;
	void SetSettings(ChunkSettings settings);
	/// Convenient opt in: world pixel size; (0,0) disables explicit chunks only.
	void SetChunkSize(V2_int world_size);
	void Disable();
	[[nodiscard]] V2_int WorldToChunk(V2_float world_position) const;
	[[nodiscard]] bool IsLoaded(V2_int coordinate) const;
	[[nodiscard]] std::size_t GetLoadedCount() const;
	[[nodiscard]] bool OwnsEntity(Entity entity) const;
	/// True for runtime owned entities, including generated tile collider proxies.
	/// Useful for filtering runtime implementation details out of the editor hierarchy.
	[[nodiscard]] bool IsManagedRuntimeEntity(Entity entity) const;
	/// Generated entities outside simulation distance are omitted by built in update systems.
	[[nodiscard]] bool ShouldSimulate(Entity entity) const;
	[[nodiscard]] bool ShouldRender(Entity entity) const;
	/// Uses the camera receiving the draw command, rather than the primary camera.
	[[nodiscard]] bool ShouldRender(Entity entity, SceneCamera camera) const;
	/// Explicit opt out for global world entities and camera independent controllers.
	/// UI entities and camera entities are already exempt automatically.
	void SetAlwaysActive(Entity entity, bool active = true);
	/// Opt into a policy explicitly, including for editor authored objects during runtime.
	void SetEntityPolicy(Entity entity, WorldEntityPolicy policy);
	/// Called by Scene::CreateEntity; normal runtime created roots are streamed automatically.
	void OnEntityCreated(Entity entity);
	/// Called before OnEnter, so entities created by OnEnter are tracked.
	void BeginRuntime();

	/// Stable IDs and absolute lattice coordinates make regeneration deterministic. A callback
	/// should create one entity or return null, and must not depend on generation order.
	void AddEntityGenerator(
		std::string id, V2_float spacing, EntityFactory factory, V2_float origin = {}
	);
	/// Tilemap generators use the tilemap's own cell size and world transform.
	void AddTileGenerator(std::string id, Tilemap tilemap, TileFactory factory);
	void ClearGenerators();

	/// Set a directory of sparse per chunk delta files. Streaming saves changes on eviction,
	/// and restores saved deltas before generating a chunk. Empty disables disk persistence.
	/// On the web, directories are rooted at /tmp/protegon_chunks (ephemeral MEMFS).
	void SetSaveDirectory(path directory);
	[[nodiscard]] const path& GetSaveDirectory() const;
	/// Backward compatible configuration: "saves/world/chunks.json" uses the sibling
	/// "saves/world/chunks/" directory. Existing monolithic journals are migrated once.
	/// A path without .json is treated as a directory.
	void SetSavePath(path filepath);
	[[nodiscard]] bool LoadFromFile(const path& filepath);
	[[nodiscard]] bool SaveToFile(const path& filepath);
	[[nodiscard]] bool SaveConfigured();
	/// Editor play sessions can read existing chunk saves without changing any files.
	/// Disabling writes retains in memory deltas for the current runtime only.
	void SetDiskWritesEnabled(bool enabled);
	[[nodiscard]] bool AreDiskWritesEnabled() const;
	/// Whether a saved delta removed a generated entity. Used by the editor preview
	/// without generating ECS instances; results are cached by streaming page.
	[[nodiscard]] bool IsSavedGeneratedEntityRemoved(
		std::string_view generator_id, V2_int cell, V2_float generated_anchor
	) const;
	/// Discard all saved chunk deltas and reset resident generated content to its
	/// deterministic baseline. Intended for editor reset actions; does not touch
	/// authored scene entities or any unrelated files in the save directory.
	[[nodiscard]] bool ResetSavedChanges(Scene& scene);
	/// Destroy a streamed entity and track its removal in the owning chunk.
	/// Disk writes are coalesced and flushed shortly afterward (or on eviction/exit).
	/// For unowned entities, behaves like Entity::Destroy without disk I/O.
	/// Use for harvesting instead of scanning the world with SaveConfigured().
	[[nodiscard]] bool DestroyAndPersist(Entity entity);
	[[nodiscard]] json SerializeChanges();
	[[nodiscard]] bool DeserializeChanges(const json& changes);

	/// Runtime scene lifecycle. World camera is used as the streaming focus.
	void Update(Scene& scene);
	void UnloadAll(Scene& scene);
	/// Called by Tilemap mutators before an authored/runtime tile edit. Direct writes to
	/// TilemapData::tiles bypass tracking; use SetTile/EraseTile for streamed edits.
	void BeforeTileChanged(Tilemap tilemap, V2_int cell);
	/// Automatically maintains static Collider proxies for collidable tilemap tiles.
	void AfterTileChanged(Tilemap tilemap, V2_int cell);

private:
	struct EntityGenerator {
		std::string id;
		V2_float spacing;
		V2_float origin;
		EntityFactory factory;
	};

	struct TileGenerator {
		std::string id;
		UUID tilemap_uuid;
		TileFactory factory;
	};

	struct GeneratedEntity {
		std::string key;
		Entity entity;
		int uuid{ 0 };
		json baseline;
	};

	struct SpawnedEntity {
		std::string key;
		Entity entity;
		int uuid{ 0 };
	};

	struct ManagedTile {
		Tilemap tilemap;
		V2_int cell;
		std::string key;
		std::optional<TilemapTile> baseline;
		std::optional<TilemapTile> restore;
	};

	struct TileCollider {
		Entity entity;
		Tilemap tilemap;
		V2_int cell;
		std::string key;
		int uuid{ 0 };
	};

	struct Chunk {
		/// Last on disk delta, so periodic autosaves only write changed chunks.
		json last_saved_delta{ json::object() };
		std::vector<GeneratedEntity> entities;
		std::vector<SpawnedEntity> spawned;
		std::vector<ManagedTile> tiles;
		std::unordered_set<std::string> tile_keys;
		std::vector<TileCollider> colliders;
	};

	[[nodiscard]] std::string TileKey(Tilemap tilemap, V2_int cell) const;
	[[nodiscard]] bool LoadChunk(Scene& scene, V2_int coordinate);
	[[nodiscard]] bool PrepareStorage();
	[[nodiscard]] path ChunkFile(V2_int coordinate) const;
	[[nodiscard]] bool ReadChunkDelta(V2_int coordinate, json& delta) const;
	[[nodiscard]] bool EnsureDeltaLoaded(V2_int coordinate);
	[[nodiscard]] bool WriteChunkDelta(V2_int coordinate, const json& delta);
	void PopulateColliders(Scene& scene, V2_int coordinate);
	void RebuildColliders(Scene& scene, V2_int coordinate);
	void ClearColliders(Chunk& chunk);
	void RegisterPaintGenerators(Scene& scene);
	void AdoptNewEntities(Scene& scene);
	void MigrateEntities(Scene& scene);
	void SyncTileCollider(Tilemap tilemap, V2_int cell);
	void SnapshotChunk(V2_int coordinate, Chunk& chunk);
	void UnloadChunk(V2_int coordinate, Scene& scene);
	void AddManagedTile(
		Chunk& chunk, Tilemap tilemap, V2_int cell, std::optional<TilemapTile> baseline,
		std::optional<TilemapTile> restore
	);
	[[nodiscard]] std::optional<V2_int> OwnedCoordinate(Entity entity) const;
	[[nodiscard]] bool IsSimulationChunk(V2_int coordinate) const;
	[[nodiscard]] V2_int EffectiveSize() const;
	[[nodiscard]] bool IsStreaming() const;
	[[nodiscard]] bool IsSpatiallyVisible(Entity entity, BoundingAABB bounds) const;
	void UpdateViewBounds(SceneCamera camera);
	void UpdateViewBounds(const Camera& camera, V2_int logical_size);

	ChunkSettings settings_{};
	std::vector<EntityGenerator> entity_generators_{};
	std::vector<TileGenerator> tile_generators_{};
	std::unordered_map<V2_int, Chunk> chunks_{};
	/// Constant time lookup for generated entity simulation and scene serialization.
	std::unordered_map<int, V2_int> owned_entities_{};
	std::unordered_set<int> pending_new_entities_{};
	bool tracking_runtime_{ false };
	bool suppress_creation_tracking_{ false };
	bool paint_generators_registered_{ false };
	bool rebuilding_colliders_{ false };
	std::unordered_map<std::string, Entity> collider_proxies_{};
	// Only used without explicit chunks when procedural content or tilemaps need paging.
	bool viewport_pages_{ false };
	static constexpr V2_int kViewportPageSize{ 512, 512 };
	BoundingAABB render_bounds_{};
	BoundingAABB simulation_bounds_{};
	bool view_bounds_valid_{ false };
	std::unordered_set<int> always_active_{};
	/// Delta dictionaries: "entities", "tiles", and full UUID preserving "spawned" snapshots.
	std::unordered_map<V2_int, json> changes_{};
	// Read only edit mode preview: don't reparse a page for every generated sprite.
	mutable std::unordered_map<V2_int, json> preview_saved_deltas_{};
	/// Coalesce gameplay removals and limit disk commits to one page per tick.
	std::unordered_set<V2_int> pending_commits_{};
	std::chrono::steady_clock::time_point next_pending_commit_{};
	path save_directory_{};
	path legacy_save_path_{};
	bool storage_prepared_{ false };
	bool disk_writes_enabled_{ true };
	std::chrono::steady_clock::time_point last_auto_save_{ std::chrono::steady_clock::now() };
	std::unordered_set<V2_int> corrupt_chunks_{};
	V2_int simulation_min_{};
	V2_int simulation_max_{};
	V2_int render_min_{};
	V2_int render_max_{};
	bool modifying_tiles_{ false };
};

} // namespace ptgn
