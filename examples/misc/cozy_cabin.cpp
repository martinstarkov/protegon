#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/noise.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/world/chunk.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"

using namespace ptgn;

namespace {

constexpr V2_int kWindowSize{ 1280, 720 };
constexpr V2_int kTileSize{ 16, 16 };
constexpr V2_int kChunkSize{ 128, 128 };
constexpr V2_float kTreeSpacing{ 32.0f, 32.0f };
constexpr V2_float kMushroomSpacing{ 32.0f, 32.0f };
constexpr V2_float kTreeOrigin{ 16.0f, 16.0f };
constexpr V2_float kMushroomOrigin{ 16.0f, 16.0f };
constexpr V2_float kPlayerSpawn{ -30.0f, 7.0f };
constexpr float kTreeInteractRadius{ 15.0f };
constexpr float kMushroomInteractRadius{ 13.0f };

// Generator IDs and cell keys remain stable for sparse saved removals.
constexpr std::int32_t kTerrainSeed{ 2105 };
constexpr std::int32_t kTreesSeed{ 7213 };
constexpr std::int32_t kMushroomsSeed{ 4139 };

// A per-cell hash is used only for local density variation; Perlin noise defines
// continuous large-scale patches. No global RNG or generation-order dependencies.
std::uint32_t CellHash(V2_int cell, std::uint32_t salt) {
	std::uint32_t x{ static_cast<std::uint32_t>(cell.x) };
	std::uint32_t y{ static_cast<std::uint32_t>(cell.y) };
	std::uint32_t hash{ salt ^ (x * 0x85ebca6bu) ^ (y * 0xc2b2ae35u) };
	hash ^= hash >> 16u;
	hash *= 0x7feb352du;
	hash ^= hash >> 15u;
	hash *= 0x846ca68bu;
	hash ^= hash >> 16u;
	return hash;
}

float HashUnit(V2_int cell, std::uint32_t salt) {
	return static_cast<float>(CellHash(cell, salt) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

// The procedural key is the stable integer cell. Visually the sprite can be
// anywhere within that cell, rather than being snapped to a 32-unit grid.
// The displacement cannot cross a streaming page border because 128 is an
// exact multiple of 32 and every anchor is at its cell center.
V2_float JitterPosition(V2_int cell, V2_float spacing, V2_float origin, std::uint32_t salt) {
	V2_float anchor{ origin + V2_float{ static_cast<float>(cell.x) * spacing.x,
										static_cast<float>(cell.y) * spacing.y } };
	return anchor +
		   V2_float{ (HashUnit(cell, salt) * 2.0f - 1.0f) * spacing.x * 0.46f,
					 (HashUnit(cell, salt ^ 0x5e19c713u) * 2.0f - 1.0f) * spacing.y * 0.46f };
}

// Generator noise sources are editable in the Paint Generator inspector. This
// selection uses the same noise sampler and parameters as the engine's generator.
bool ResourceGrows(const PaintGeneratorRecipe& recipe, V2_float position, V2_int cell,
	std::uint32_t salt) {
	if (recipe.source_kind != PaintGeneratorSourceKind::Noise) {
		return false;
	}

	float value{ SamplePaintGeneratorNoise(recipe.noise, position) };
	bool in_region{ false };

	for (const auto& region : recipe.noise.thresholds) {
		if (!region.enabled) {
			continue;
		}

		float lower{ std::min(region.minimum, region.maximum) };
		float upper{ std::max(region.minimum, region.maximum) };

		if (value >= lower && (value < upper || (upper >= 1.0f && value <= 1.0f))) {
			in_region = true;
			break;
		}
	}

	// Noise determines the organic grove/patch boundaries; the stable cell hash
	// thins candidates without introducing any generation-order dependence.
	return in_region && HashUnit(cell, salt) < std::clamp(recipe.density, 0.0f, 1.0f);
}

PaintGeneratorNoise SnowNoise() {
	PaintGeneratorNoise noise;
	noise.type = NoiseType::Perlin;
	noise.seed = kTerrainSeed;
	noise.frequency = 0.045f;
	noise.octaves = 3;
	return noise;
}

Color SnowTint(V2_int cell, const PaintGeneratorNoise& noise_source) {
	float noise{ SamplePaintGeneratorNoise(
		noise_source, V2_float{ static_cast<float>(cell.x), static_cast<float>(cell.y) }
	) };
	return noise < 0.40f ? Color{ 215, 231, 236, 255 } :
		noise > 0.60f ? Color{ 250, 250, 255, 255 } : Color{ 231, 241, 247, 255 };
}

bool NearHouse(V2_float position) {
	// Leave an open, authored clearing around the pre-placed house, including the entrance.
	return std::abs(position.x) < 132.0f && std::abs(position.y) < 132.0f;
}

struct HouseWall {
	V2_float top_left;
	V2_float size;
};

// Reused from the original jam's resources/data/data.json (house_hitboxes).
constexpr std::array<HouseWall, 8> kHouseWalls{ {
	{ { 2, 40 }, { 100, 32 } },
	{ { 2, 72 }, { 6, 16 } },
	{ { 2, 104 }, { 6, 32 } },
	{ { 8, 111 }, { 15, 25 } },
	{ { 2, 136 }, { 166, 40 } },
	{ { 96, 0 }, { 6, 40 } },
	{ { 102, 0 }, { 66, 42 } },
	{ { 162, 42 }, { 6, 94 } },
} };

} // namespace

class CozyCabinScene : public Scene {
public:
	void OnNew() override {
		// Register assets before creating sprite entities so the new editor
		// project's authored house and player serialize with valid textures.
		LoadAssets();

		// These settings are authored once. The editor can subsequently change
		// them and the project scene will retain those edits.
		ChunkSettings settings{ ctx().chunks.GetSettings() };
		settings.world_size			  = kChunkSize;
		settings.render_viewports	  = 1.5f;
		settings.simulation_viewports = 1.5f;
		settings.render_distance	  = 1;
		settings.simulation_distance  = 1;
		settings.keep_alive_distance  = 2;
		settings.max_loaded_chunks	  = 56;
		settings.save_directory		  = "CozyCabinProject/saves/cozy_cabin/chunks";
        settings.save_mode = ChunkSaveMode::Automatic;
        settings.autosave_interval = 15s;
		ctx().chunks.SetSettings(settings);
		SetBackgroundColor(Color{ 228, 239, 244, 255 });

		// Authored content: created once by the scene, never owned by a procedural chunk.
		Tilemap ground{ CreateTilemap(*this, Tag{ "Snow Ground" }) };
		ground.SetCellSize(V2_float{ kTileSize });
		SetDepth(ground, -100.0f);

		Entity house{ CreateSprite(*this, V2_float{}, TextureKey{ "cabin_house" }) };
		house.Add<Tag>(Tag{ "House" });
		SetDepth(house, 10.0f);

		constexpr V2_float house_top_left{ -84.0f, -88.0f };
		for (const auto& wall : kHouseWalls) {
			Entity entity{ CreateEntity(Tag{ "House Wall" }) };
			entity.Add<Transform>(house_top_left + wall.top_left + wall.size * 0.5f);
			entity.Add<Collider>(Rect{ wall.size });
		}

		player_ = CreateSprite(*this, kPlayerSpawn, TextureKey{ "cabin_player" });
		player_.Add<Tag>(Tag{ "Player" });
		player_.Add<Collider>(Rect{ V2_float{ 10.0f, 10.0f } })
			.SetCollisionMode(CollisionMode::Continuous);
		player_.Add<RigidBody>();
		auto& movement{ player_.Add<TopDownMovement>() };
		movement.use_acceleration = false;
		movement.max_speed		  = 105.0f;
		SetDepth(player_, 30.0f);

		// The editor's authored scene camera must already match the runtime view.
		ctx().camera.SetZoom(4.0f);
		ctx().camera.SetPixelRounding(true);
		SetPosition(ctx().camera, kPlayerSpawn);

		// These two scene entities are the entire authored forest/mushroom
		// generators. Their runtime instances are deliberately NOT scene roots.
		CreateGenerator("Infinite Trees", "cozy_cabin:trees", kTreeSpacing);
		CreateGenerator("Infinite Mushrooms", "cozy_cabin:mushrooms", kMushroomSpacing);
	}

	void OnLoad() override {
		LoadAssets();

		// These generators are authored by OnNew and can be edited or deleted in
		// the scene. Loading never reconstructs a deleted generator.
		PaintGenerator tree_source{ GetEntity(Tag{ "Infinite Trees" }) };
		PaintGenerator mushroom_source{ GetEntity(Tag{ "Infinite Mushrooms" }) };

		player_ = GetEntity(Tag{ "Player" });
		Tilemap ground{ GetEntity(Tag{ "Snow Ground" }) };

		// Seamless 16x16 snow terrain, sampled using the generator noise API.
		PaintGeneratorNoise snow_noise{ SnowNoise() };
		ctx().chunks.AddTileGenerator(
			"cozy_cabin:snow", ground, [snow_noise](V2_int cell) -> std::optional<TilemapTile> {

				TilemapTile tile;
				tile.coordinate = cell;
				tile.texture	= TextureKey{ "cabin_snow" };
				tile.pixel_size = kTileSize;
				tile.origin		= Origin::TopLeft;
				tile.collidable = false;
				// Subtle snowy shading without fragmenting the ground into entities.
				tile.tint = SnowTint(cell, snow_noise);
				return tile;
			}
		);

		// Cell keys remain stable while sprites get near-full-cell world-space jitter.
		// ChunkManager uses the keys to suppress previously harvested resources.
		if (tree_source && tree_source.Has<WorldGeneratorSource>() &&
			tree_source.GetData().enabled &&
			tree_source.GetData().stream_at_runtime.value_or(true)) {
			V2_float spacing{ tree_source.GetData().grid_size };
			V2_float origin{ kTreeOrigin + tree_source.GetData().grid_offset };
			PaintGeneratorRecipe recipe{ tree_source.GetData().recipe };
			ctx().chunks.AddEntityGenerator(
				tree_source.Get<WorldGeneratorSource>().id, spacing,
				[spacing, origin, recipe](Scene& scene, V2_int cell) -> Entity {
					V2_float position{ JitterPosition(cell, spacing, origin, 0xf38a179du) };
					if (NearHouse(position)) {
						return {};
					}

					if (!ResourceGrows(recipe, position, cell, 0x1234eaa1u)) {
						return {};
					}

					Entity tree{ CreateSprite(scene, position, TextureKey{ "cabin_tree" }) };
					tree.Add<Tag>(Tag{ "Winter Tree" });
					// Solid trunk at the base of the 24x32 sprite, not the entire canopy.
					tree.Add<Collider>(Rect{ V2_float{ -6.0f, 6.0f }, V2_float{ 6.0f, 16.0f } });
					SetDepth(tree, 20.0f);
					return tree;
				},
				origin
			);
		}

		if (mushroom_source && mushroom_source.Has<WorldGeneratorSource>() &&
			mushroom_source.GetData().enabled &&
			mushroom_source.GetData().stream_at_runtime.value_or(true)) {
			V2_float spacing{ mushroom_source.GetData().grid_size };
			V2_float origin{ kMushroomOrigin + mushroom_source.GetData().grid_offset };
			PaintGeneratorRecipe recipe{ mushroom_source.GetData().recipe };
			ctx().chunks.AddEntityGenerator(
				mushroom_source.Get<WorldGeneratorSource>().id, spacing,
				[spacing, origin, recipe](Scene& scene, V2_int cell) -> Entity {
					V2_float position{ JitterPosition(cell, spacing, origin, 0x4cdd8319u) };
					if (NearHouse(position)) {
						return {};
					}

					if (!ResourceGrows(recipe, position, cell, 0x78245cfdu)) {
						return {};
					}

					Entity mushroom{
						CreateSprite(scene, position, TextureKey{ "cabin_mushroom" })
					};
					mushroom.Add<Tag>(Tag{ "Winter Mushroom" });
					SetDepth(mushroom, 15.0f);
					// Intentionally no Collider: picking uses the distance-based interaction.
					return mushroom;
				},
				origin
			);
		}
	}

	void OnEnter() override {
		player_ = GetEntity(Tag{ "Player" });
		// The player should not be unloaded or frozen by chunk residency.
		ctx().chunks.SetEntityPolicy(player_, WorldEntityPolicy::Global);
		// Zoom and pixel alignment are authored in OnNew so edit and play match.
		ctx().camera.SetPixelRounding(true);
		SetPosition(ctx().camera, GetPosition(player_));
		ctx().renderer.SetBackgroundColor(Color{ 228, 239, 244, 255 });
	}

	void OnUpdate() override {
		if (!player_) {
			return;
		}

		// TopDownMovement and collision update later in this frame. Camera
		// placement and UI drawing are performed by OnRender after physics.
		nearest_resource_ = {};
		resource_prompt_ = {};
		Entity nearest;
		std::string_view prompt;
		float nearest_distance2{ kTreeInteractRadius * kTreeInteractRadius };
		V2_float player_position{ GetWorldPosition(player_) };

		for (auto [entity, tag, transform] : EntitiesWith<Tag, Transform>()) {
			if (!entity || (tag.value != "Winter Tree" && tag.value != "Winter Mushroom")) {
				continue;
			}

            bool tree{ tag.value == "Winter Tree" };
            float radius{ tree ? kTreeInteractRadius : kMushroomInteractRadius };
            // Tree interaction is centered at the base/trunk, not its canopy.
            V2_float interaction_center{ transform.position +
                (tree ? V2_float{ 0.0f, 9.0f } : V2_float{}) };
            V2_float difference{ interaction_center - player_position };
            float distance2{ difference.MagnitudeSquared() };

            if (distance2 >= radius * radius || distance2 >= nearest_distance2) {
				continue;
			}

			nearest_distance2 = distance2;
			nearest			  = entity;
			prompt			  = tag.value == "Winter Tree" ? "[E] Chop tree" : "[E] Pick mushroom";
		}

		if (!nearest) {
			return;
		}

		if (ctx().input.KeyPressed(Key::E)) {
			// Persist only the harvested resource's owning chunk, not the entire
			// resident world. Disk writes are coalesced and flushed on exit.
			if (!ctx().chunks.DestroyAndPersist(nearest)) {
				PTGN_WARN("Cozy Cabin: failed to persist harvested resource");
			}
			return;
		}

		nearest_resource_ = nearest;
		resource_prompt_ = prompt;
	}

	void OnRender() override {
		if (!IsRuntime()) {
			DrawEditorWorldPreview();
			return;
		}

		if (!player_) {
			return;
		}

		// The physics step runs after OnUpdate. Following here eliminates the
		// one-frame camera lag that made the player appear to jitter while moving.
		SetPosition(ctx().camera, GetWorldPosition(player_));

		V2_float logical{ V2_float{ ctx().renderer.GetLogicalSize() } };
		V2_float hud_position{ -logical * 0.5f + V2_float{ 120.0f, 22.0f } };
		ctx().render_queue.DrawText(
			Transform{ hud_position }, "WASD move   E interact", Color{ 38, 51, 62, 255 },
			11.0f, {}, TextRenderParams{
				.origin = Origin::Center, .depth = Depth{ 100.0f },
				.camera = GetFixedCamera() }
		);

		if (!nearest_resource_) {
			return;
		}

		// Center horizontally on the sprite's visual center, above its top edge.
		// Scale the glyph size, not the whole transform: scaling a text transform
		// otherwise displaces its anchor towards the top-left as zoom changes.
		V2_float size{ GetDisplaySize(nearest_resource_).value_or(V2_float{ 24.0f, 32.0f }) };
		V2_float resource_position{ GetWorldPosition(nearest_resource_) };
		resource_position.y -= size.y * 0.5f + 6.0f;
		float zoom{ ctx().camera.GetZoom().x };
		ctx().render_queue.DrawText(
			Transform{ resource_position }, resource_prompt_, Color{ 38, 51, 62, 255 },
			11.0f / std::max(zoom, 0.01f), {}, TextRenderParams{
				.origin = Origin::Center, .depth = Depth{ 100.0f },
				.camera = ctx().camera }
		);
	}

private:
	// Show the actual deterministic procedural output in edit mode without
	// creating ECS instances or loading chunks. Only sample visible cells.
	void DrawEditorWorldPreview() {
		const auto& override_camera{ ctx().renderer.GetPrimaryWorldCamera() };
		auto vertices{ override_camera.has_value()
			? override_camera->GetWorldVertices(ctx().renderer.GetLogicalSize())
			: ctx().camera.GetWorldVertices() };
		V2_float lower{ vertices[0] };
		V2_float upper{ vertices[0] };
		for (V2_float corner : vertices) {
			lower.x = std::min(lower.x, corner.x);
			lower.y = std::min(lower.y, corner.y);
			upper.x = std::max(upper.x, corner.x);
			upper.y = std::max(upper.y, corner.y);
		}
		const PaintGeneratorNoise snow_noise{ SnowNoise() };
		// Cache deterministic tint samples across editor frames. Moving the camera
		// should not recompute thousands of noise octaves every render.
		if (snow_preview_tints_.size() > 16384) {
			snow_preview_tints_.clear();
		}
		int min_x{ static_cast<int>(std::floor(lower.x / kTileSize.x)) - 1 };
		int min_y{ static_cast<int>(std::floor(lower.y / kTileSize.y)) - 1 };
		int max_x{ static_cast<int>(std::floor(upper.x / kTileSize.x)) + 1 };
		int max_y{ static_cast<int>(std::floor(upper.y / kTileSize.y)) + 1 };
		// Protect the editor against enormous zoom-out viewports.
		if (static_cast<long long>(max_x - min_x + 1) * (max_y - min_y + 1) <= 8192) {
			for (int y{ min_y }; y <= max_y; ++y) {
				for (int x{ min_x }; x <= max_x; ++x) {
					V2_int cell{ x, y };
					V2_float world{ static_cast<float>(x * kTileSize.x),
						static_cast<float>(y * kTileSize.y) };
					auto tint_it{ snow_preview_tints_.find(cell) };
					if (tint_it == snow_preview_tints_.end()) {
						tint_it = snow_preview_tints_.emplace(cell, SnowTint(cell, snow_noise)).first;
					}
					ctx().render_queue.DrawTexture(
						Transform{ world }, TextureKey{ "cabin_snow" },
						TextureRenderParams{
							.size = V2_float{ kTileSize }, .origin = Origin::TopLeft,
							.tint = tint_it->second, .depth = Depth{ -100.0f }
						}
					);
				}
			}
		}

		auto preview_resources = [&](std::string_view tag, V2_float origin,
			std::uint32_t jitter_salt, std::uint32_t density_salt,
			TextureKey texture, V2_float texture_size, float depth) {
			PaintGenerator generator{ GetEntity(Tag{ std::string{ tag } }) };
			if (!generator || !generator.GetData().enabled ||
				!generator.GetData().grid_size.IsPositive()) {
				return;
			}
			const auto& data{ generator.GetData() };
			V2_float spacing{ data.grid_size };
			V2_float anchor{ origin + data.grid_offset };
			int first_x{ static_cast<int>(std::floor((lower.x - anchor.x - texture_size.x) / spacing.x)) };
			int first_y{ static_cast<int>(std::floor((lower.y - anchor.y - texture_size.y) / spacing.y)) };
			int last_x{ static_cast<int>(std::ceil((upper.x - anchor.x + texture_size.x) / spacing.x)) };
			int last_y{ static_cast<int>(std::ceil((upper.y - anchor.y + texture_size.y) / spacing.y)) };
			if (static_cast<long long>(last_x - first_x + 1) *
				(last_y - first_y + 1) > 4096) {
				return;
			}
			for (int y{ first_y }; y <= last_y; ++y) {
				for (int x{ first_x }; x <= last_x; ++x) {
					V2_int cell{ x, y };
					V2_float point{ JitterPosition(cell, spacing, anchor, jitter_salt) };
					if (NearHouse(point) || generator.IsSuppressed(cell) ||
						!ResourceGrows(data.recipe, point, cell, density_salt)) {
						continue;
					}
					ctx().render_queue.DrawTexture(
						Transform{ point }, texture,
						TextureRenderParams{
							.size = texture_size, .origin = Origin::Center,
							.depth = Depth{ depth }
						}
					);
				}
			}
		};
		preview_resources("Infinite Trees", kTreeOrigin, 0xf38a179du,
			0x1234eaa1u, TextureKey{ "cabin_tree" }, { 24.0f, 32.0f }, 20.0f);
		preview_resources("Infinite Mushrooms", kMushroomOrigin, 0x4cdd8319u,
			0x78245cfdu, TextureKey{ "cabin_mushroom" }, { 16.0f, 16.0f }, 15.0f);
	}

	void CreateGenerator(std::string_view name, std::string id, V2_float spacing) {
		PaintGenerator generator{ CreatePaintGenerator(*this, Tag{ std::string{ name } }) };
		generator.SetGeometry(PaintGeneratorGeometry::Infinite);
		generator.SetGrid(spacing);
		generator.GetData().stream_at_runtime = true;
		auto& recipe{ generator.GetData().recipe };
		recipe.source_kind = PaintGeneratorSourceKind::Noise;
		recipe.noise.type = NoiseType::Perlin;
		recipe.noise.seed = id == "cozy_cabin:trees" ? kTreesSeed : kMushroomsSeed;
		recipe.noise.frequency = id == "cozy_cabin:trees" ? 0.009f : 0.014f;
		recipe.noise.octaves = 3;
		recipe.noise.thresholds = { PaintGeneratorNoiseThreshold{
			.minimum = id == "cozy_cabin:trees" ? 0.48f : 0.52f,
			.maximum = 1.0f } };
		recipe.density = id == "cozy_cabin:trees" ? 0.58f : 0.38f;

		// The authored generator carries settings and identity; its generated
		// resource instances belong to the runtime stream, not the scene file.
		auto& source{ generator.Add<WorldGeneratorSource>() };
		source.id = std::move(id);
		const bool trees{ source.id == "cozy_cabin:trees" };
		// Preview metadata is generic and shared by the editor selection overlay.
		source.preview_texture = TextureKey{ trees ? "cabin_tree" : "cabin_mushroom" };
		source.preview_size = trees ? V2_float{ 24.0f, 32.0f } : V2_float{ 16.0f, 16.0f };
		source.preview_origin = trees ? kTreeOrigin : kMushroomOrigin;
		source.preview_jitter_salt = trees ? 0xf38a179du : 0x4cdd8319u;
		source.preview_density_salt = trees ? 0x1234eaa1u : 0x78245cfdu;
		source.preview_exclusion_half_size = V2_float{ 132.0f, 132.0f };
	}

	void LoadAssets() {
		// Examples register examples/assets as their asset directory in CMake.
		// AssetManager resolves paths beginning with "assets/" against that
		// directory, regardless of the process working directory or build folder.
		constexpr std::array<std::pair<std::string_view, std::string_view>, 5> files{
			{ { "cabin_snow", "snow.png" },
			  { "cabin_tree", "tree.png" },
			  { "cabin_mushroom", "mushroom.png" },
			  { "cabin_house", "house.png" },
			  { "cabin_player", "player_temp.png" } }
		};

		for (const auto& [key, filename] : files) {
			TextureKey texture_key{ std::string{ key } };

			if (ctx().asset.Has(texture_key)) {
				continue;
			}

			path filepath{ path{ "assets/cozy_cabin" } / filename };
			ctx().asset.Load(texture_key, filepath);

			if (!ctx().asset.Has(texture_key)) {
				PTGN_WARN("Cozy Cabin: failed to load texture: ", filepath.generic_string());
			}
		}
	}

	std::unordered_map<V2_int, Color> snow_preview_tints_{};
	Entity player_{};
	Entity nearest_resource_{};
	std::string_view resource_prompt_{};
};

PTGN_REGISTER_SCENE(CozyCabinScene);

int main(int, char**) {
	Application app{ "Cozy Cabin: Infinite World", kWindowSize };
#if defined(__EMSCRIPTEN__)
	// Browser builds can run without a pre-baked editor project file.
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<CozyCabinScene>();
#else
	// On first launch, StartProject creates a real .ptgnproj and editable
	// .ptgnscene containing the authored house, walls, player and tilemap.
	// Subsequent launches reopen the same project without rebuilding it.
	PTGN_WITH_EDITOR(app, true);
	app.StartProject<CozyCabinScene>("CozyCabinProject/CozyCabin.ptgnproj");
#endif
}
