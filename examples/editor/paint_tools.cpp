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

constexpr std::string_view kTerrainTilesetTexture{ "terrain_tileset" };

constexpr std::string_view kHumanTexture{ "animation" };

constexpr std::string_view kSmileTexture{ "smile" };

constexpr std::string_view kHumanPrefabName{ "Animated Human" };

constexpr std::string_view kSmilePrefabName{ "Smiley Face" };

constexpr int kTilesetWidth{ 80 };

constexpr int kTilesetHeight{ 96 };

constexpr int kTileSize{ 16 };

void LoadAssets(Scene& scene) {
	auto& assets{ scene.ctx().asset };
	// The _16x16 suffix is consumed by PaintEditor so this atlas is automatically sliced
	// into 16x16 cells and becomes available in the Autotile / Terrain tilesheet combo.
	assets.Load(kTerrainTilesetTexture, "assets/terrain_tileset_16x16.png");
	assets.Load(kHumanTexture, "assets/animation_frames4x3.png");
	assets.Load(kSmileTexture, "assets/smile.png");
}

struct DemoPrefabKeys {
	PrefabKey human{};
	PrefabKey smile{};
};

DemoPrefabKeys EnsureDemoPrefabs(Scene& scene) {
	return {
		.human = scene.EnsurePrefabAsset(kHumanPrefabName, [](Scene& authoring_scene) -> Entity {
			Animation human{
				CreateAnimation(
					authoring_scene,
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
			SetScale(human, 2.0f);
			return human;
		}),
		.smile = scene.EnsurePrefabAsset(kSmilePrefabName, [](Scene& authoring_scene) -> Entity {
			Sprite smile{
				CreateSprite(
					authoring_scene,
					{},
					TextureKey{ std::string{ kSmileTexture } },
					Origin::Center
				)
			};
			smile.Add<Tag>(std::string{ kSmilePrefabName });
			return smile;
		}),
	};
}

std::array<V2_float, 4> TilesetUVs(V2_int slice) {
	const float left{
		static_cast<float>(slice.x * kTileSize) / static_cast<float>(kTilesetWidth)
	};
	const float top{
		static_cast<float>(slice.y * kTileSize) / static_cast<float>(kTilesetHeight)
	};
	const float right{
		static_cast<float>((slice.x + 1) * kTileSize) / static_cast<float>(kTilesetWidth)
	};
	const float bottom{
		static_cast<float>((slice.y + 1) * kTileSize) / static_cast<float>(kTilesetHeight)
	};
	return {
		V2_float{ left, top },
		V2_float{ right, top },
		V2_float{ right, bottom },
		V2_float{ left, bottom },
	};
}

TilemapTile MakeTilesetTile(
	V2_int coordinate,
	V2_int slice,
	Color tint = color::White
) {
	TilemapTile tile;
	tile.coordinate = coordinate;
	tile.texture = TextureKey{ std::string{ kTerrainTilesetTexture } };
	tile.texture_coordinates = TilesetUVs(slice);
	tile.pixel_size = { kTileSize, kTileSize };
	tile.origin = Origin::TopLeft;
	tile.tint = tint;
	return tile;
}

void SeedBorderedArea(
	Tilemap& tilemap,
	V2_int top_left,
	V2_int size,
	V2_int top_left_slice,
	V2_int top_slice,
	V2_int top_right_slice,
	V2_int left_slice,
	V2_int center_slice,
	V2_int right_slice,
	V2_int bottom_left_slice,
	V2_int bottom_slice,
	V2_int bottom_right_slice
) {
	for (int y{}; y < size.y; ++y) {
		for (int x{}; x < size.x; ++x) {
			const bool left{ x == 0 };
			const bool right{ x == size.x - 1 };
			const bool top{ y == 0 };
			const bool bottom{ y == size.y - 1 };
			V2_int slice{ center_slice };
			if (top && left) {
				slice = top_left_slice;
			} else if (top && right) {
				slice = top_right_slice;
			} else if (bottom && left) {
				slice = bottom_left_slice;
			} else if (bottom && right) {
				slice = bottom_right_slice;
			} else if (top) {
				slice = top_slice;
			} else if (bottom) {
				slice = bottom_slice;
			} else if (left) {
				slice = left_slice;
			} else if (right) {
				slice = right_slice;
			}
			tilemap.SetTile(
				MakeTilesetTile(top_left + V2_int{ x, y }, slice)
			);
		}
	}
}

void SeedGroundTiles(Tilemap& tilemap) {
	// Main 9x7 bordered patch using the upper 3x3 terrain set.
	//
	// Atlas slices:
	//   TL = (4,0), T = (1,0), TR = (2,0)
	//   L  = (0,1), C = (1,1), R  = (2,1)
	//   BL = (0,2), B = (1,2), BR = (2,2)
	SeedBorderedArea(
		tilemap,
		{ 0, 0 },
		{ 9, 7 },
		{ 4, 0 },
		{ 1, 0 },
		{ 2, 0 },
		{ 0, 1 },
		{ 1, 1 },
		{ 2, 1 },
		{ 0, 2 },
		{ 1, 2 },
		{ 2, 2 }
	);
	// A few authored variants from the sheet so the demo immediately shows that
	// individual atlas slices can be painted/selected independently.
	tilemap.SetTile(MakeTilesetTile({ 2, 2 }, { 3, 1 }));
	tilemap.SetTile(MakeTilesetTile({ 6, 2 }, { 3, 2 }));
	tilemap.SetTile(MakeTilesetTile({ 4, 4 }, { 4, 1 }));
}

void SeedForegroundTiles(Tilemap& tilemap) {
	// Small transparent/detail slices from the same atlas.
	tilemap.SetTile(MakeTilesetTile({ 2, 2 }, { 3, 1 }));
	tilemap.SetTile(MakeTilesetTile({ 6, 4 }, { 3, 2 }));
}

void CreateDemoLayers(Scene& scene, const DemoPrefabKeys& prefabs) {
	auto& layers{ scene.GetLayers() };
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
	ground_tilemap.SetCellSize({ 16.0f, 16.0f });
	SetPosition(ground_tilemap, { -72.0f, -56.0f });
	SeedGroundTiles(ground_tilemap);
	Tilemap foreground_tilemap{
		CreateTilemap(scene, foreground, Tag{ "Foreground Tilemap" })
	};
	foreground_tilemap.SetCellSize({ 16.0f, 16.0f });
	SetPosition(foreground_tilemap, { -72.0f, -56.0f });
	SeedForegroundTiles(foreground_tilemap);
	if (prefabs.human) {
		Entity human{ scene.CreatePrefab(prefabs.human) };
		if (human) {
			SetPosition(human, { -100.0f, 100.0f });
		}
	}
	if (prefabs.smile) {
		Entity smile{ scene.CreatePrefab(prefabs.smile) };
		if (smile) {
			SetPosition(smile, { 100.0f, 100.0f });
			(void)layers.Assign(smile, decorations, true);
		}
	}
}

} // namespace
class PaintToolsScene : public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);
		SetBackgroundColor(Color{ 28, 30, 36, 255 });
		const DemoPrefabKeys prefabs{ EnsureDemoPrefabs(*this) };
		CreateDemoLayers(*this, prefabs);
	}
	void OnLoad() override {
		LoadAssets(*this);
		// EnsurePrefabAsset is idempotent. Existing assets are reused and CreatePrefab() handles
		// residency automatically.
		(void)EnsureDemoPrefabs(*this);
	}
	void OnEnter() override {
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
