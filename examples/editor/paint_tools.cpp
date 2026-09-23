#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/world/entity_layer.h"
#include "runtime/world/tilemap.h"

using namespace ptgn;

namespace {

constexpr std::string_view kBlueTileTexture{ "blue_tile" };
constexpr std::string_view kRedTileTexture{ "red_tile" };
constexpr std::string_view kGreenTileTexture{ "green_tile" };
constexpr std::string_view kHumanTexture{ "animation" };
constexpr std::string_view kSmileTexture{ "smile" };

constexpr std::string_view kHumanPrefabName{ "Animated Human" };
constexpr std::string_view kSmilePrefabName{ "Smiley Face" };

void LoadAssets(Scene& scene) {
	auto& assets{ scene.ctx().asset };

	assets.Load(kBlueTileTexture, "assets/blue_tile.png");
	assets.Load(kRedTileTexture, "assets/red_tile.png");
	assets.Load(kGreenTileTexture, "assets/green_tile.png");
	assets.Load(kHumanTexture, "assets/animation_frames4x3.png");
	assets.Load(kSmileTexture, "assets/smile.png");
}

bool HasPrefab(const AssetManager& assets, const PrefabKey& key) {
	const auto keys{ assets.GetPrefabKeys() };

	return std::ranges::find(keys, key) != keys.end();
}

bool SavePrefabAsset(Scene& scene, Entity source, const PrefabKey& key) {
	auto& assets{ scene.ctx().asset };
	const auto project_root{ assets.GetProjectRoot() };

	if (!project_root.has_value()) {
		source.Destroy();
		return false;
	}

	const path source_path{ GetPrefabSourcePath(key) };

	assets.SavePrefab(
		CapturePrefab(source, key, true),
		project_root.value() / source_path,
		source_path
	);

	source.Destroy();
	return true;
}

void EnsureHumanPrefab(Scene& scene) {
	auto& assets{ scene.ctx().asset };
	const PrefabKey key{ MakePrefabKey(kHumanPrefabName) };

	if (HasPrefab(assets, key)) {
		return;
	}

	Animation human{
		CreateAnimation(
			scene,
			{},
			TextureKey{ std::string{ kHumanTexture } },
			AnimationConfig{
				.frame_count = 4,
				.duration = 500ms,
				.frame_size = V2_int{ 16, 32 },
				.play_count = std::nullopt,
				.start_pixel = V2_int{ 0, 32 },
			},
			Origin::Center
		)
	};

	human.Add<Tag>(std::string{ kHumanPrefabName });

	// 16x32 source frames become a convenient 32x64 paintable character.
	SetScale(human, 2.0f);

	(void)SavePrefabAsset(scene, human, key);
}

void EnsureSmilePrefab(Scene& scene) {
	auto& assets{ scene.ctx().asset };
	const PrefabKey key{ MakePrefabKey(kSmilePrefabName) };

	if (HasPrefab(assets, key)) {
		return;
	}

	Sprite smile{
		CreateSprite(
			scene,
			{},
			TextureKey{ std::string{ kSmileTexture } },
			Origin::Center
		)
	};

	smile.Add<Tag>(std::string{ kSmilePrefabName });

	(void)SavePrefabAsset(scene, smile, key);
}

void EnsureDemoPrefabs(Scene& scene) {
	EnsureHumanPrefab(scene);
	EnsureSmilePrefab(scene);

	// The temporary source entities used for CapturePrefab() were destroyed.
	// Flush them before creating visible demo instances.
	scene.Refresh();
}

TilemapTile MakeTile(
	V2_int coordinate,
	std::string_view texture,
	Color tint = color::White
) {
	TilemapTile tile;

	tile.coordinate = coordinate;
	tile.texture = TextureKey{ std::string{ texture } };
	tile.origin = Origin::TopLeft;
	tile.tint = tint;

	return tile;
}

void SeedGroundTiles(Tilemap& tilemap) {
	// Row 0: the three actual project texture assets.
	tilemap.SetTile(MakeTile({ 0, 0 }, kBlueTileTexture));
	tilemap.SetTile(MakeTile({ 1, 0 }, kRedTileTexture));
	tilemap.SetTile(MakeTile({ 2, 0 }, kGreenTileTexture));

	// Row 1: an in-code checker-style arrangement using the same paintable assets.
	tilemap.SetTile(MakeTile({ 0, 1 }, kGreenTileTexture));
	tilemap.SetTile(MakeTile({ 1, 1 }, kBlueTileTexture));
	tilemap.SetTile(MakeTile({ 2, 1 }, kRedTileTexture));

	// Row 2: code-authored tile variants. These demonstrate that TilemapTile
	// itself can carry authored tint data independently of the texture asset.
	tilemap.SetTile(MakeTile(
		{ 0, 2 },
		kBlueTileTexture,
		Color{ 255, 220, 80, 255 }
	));
	tilemap.SetTile(MakeTile(
		{ 1, 2 },
		kBlueTileTexture,
		Color{ 190, 120, 255, 255 }
	));
	tilemap.SetTile(MakeTile(
		{ 2, 2 },
		kBlueTileTexture,
		Color{ 120, 240, 220, 255 }
	));
}

void CreateDemoLayers(Scene& scene) {
	auto& layers{ scene.GetLayers() };

	// "Entities" already exists automatically as the scene's default Entity layer.
	const SceneLayerId decorations{
		layers.Create(SceneLayerKind::Entity, "Decorations")
	};

	const SceneLayerId ground{
		layers.Create(SceneLayerKind::Tile, "Ground")
	};

	const SceneLayerId foreground{
		layers.Create(SceneLayerKind::Tile, "Foreground")
	};

	Tilemap ground_tilemap{
		CreateTilemap(scene, ground, Tag{ "Ground Tilemap" })
	};

	ground_tilemap.SetCellSize({ 32.0f, 32.0f });
	SetPosition(ground_tilemap, { -48.0f, -80.0f });
	SeedGroundTiles(ground_tilemap);

	// Keep a second empty tilemap around so switching Tile layers immediately
	// demonstrates that each Tilemap owns its own authored tile set.
	Tilemap foreground_tilemap{
		CreateTilemap(scene, foreground, Tag{ "Foreground Tilemap" })
	};

	foreground_tilemap.SetCellSize({ 32.0f, 32.0f });
	SetPosition(foreground_tilemap, { -48.0f, -80.0f });

	// One prefab instance stays in the automatic/default Entity layer.
	Entity human{
		scene.CreatePrefab(MakePrefabKey(kHumanPrefabName))
	};

	if (human) {
		SetPosition(human, { -100.0f, 100.0f });
	}

	// Put the second prefab instance in another Entity layer so the demo also
	// exercises entity-layer switching, visibility and locking.
	Entity smile{
		scene.CreatePrefab(MakePrefabKey(kSmilePrefabName))
	};

	if (smile) {
		SetPosition(smile, { 100.0f, 100.0f });
		(void)layers.Assign(smile, decorations, true);
	}
}

} // namespace

class PaintToolsScene : public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);

		SetBackgroundColor(Color{ 28, 30, 36, 255 });

		EnsureDemoPrefabs(*this);
		CreateDemoLayers(*this);
	}

	void OnLoad() override {
		LoadAssets(*this);
	}

	void OnEnter() override {
		// Prefab capture stores the animation configuration, not a transient
		// "currently playing" runtime state. Start every painted animation when
		// the scene enters runtime so every Animated Human instance loops.
		for (auto [entity, _animation] : EntitiesWith<impl::AnimationData>()) {
			Animation{ entity }.Start(true);
		}
	}
};

PTGN_REGISTER_SCENE(PaintToolsScene, "Paint Tools Scene");

int main(int, char**) {
	Application app{ "Paint Tools Demo" };

	PTGN_WITH_EDITOR(app, true);

	app.StartProject<PaintToolsScene>(
		"PaintToolsProject/PaintTools.ptgnproj"
	);
}
