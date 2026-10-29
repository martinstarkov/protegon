#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/math/transform.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/world/autotile.h"
#include "runtime/world/entity_layer.h"
#include "runtime/world/tilemap.h"

using namespace ptgn;

namespace {

struct DemoTerrain {
	std::string_view name;
	std::string_view texture;
	std::string_view file;
	TilemapAutotileFormat format;
	V2_int texture_size;
	V2_float position;
};

constexpr std::array<DemoTerrain, 4> kTerrain{
	DemoTerrain{ "Classic 15",
				 "classic15_16x16",
				 "assets/autotiles/classic15_16x16.png",
				 TilemapAutotileFormat::Classic15,
				 { 80, 48 },
				 { -245.0f, -155.0f } },
	DemoTerrain{ "Blob 47",
				 "blob47_16x16",
				 "assets/autotiles/blob47_16x16.png",
				 TilemapAutotileFormat::Blob47,
				 { 128, 96 },
				 { 30.0f, -155.0f } },
	DemoTerrain{ "Subset 16",
				 "subset16_16x16",
				 "assets/autotiles/subset16_16x16.png",
				 TilemapAutotileFormat::Subset16,
				 { 64, 64 },
				 { -245.0f, 65.0f } },
	DemoTerrain{ "Dual Grid 15",
				 "dualgrid15_16x16",
				 "assets/autotiles/dualgrid15_16x16.png",
				 TilemapAutotileFormat::DualGrid15,
				 { 80, 48 },
				 { 30.0f, 65.0f } },
};

#ifdef PTGN_EDITOR
constexpr ptgn::editor::PaintAutotileFormat ToEditorFormat(TilemapAutotileFormat format) {
	switch (format) {
		case TilemapAutotileFormat::Classic15: return ptgn::editor::PaintAutotileFormat::Classic15;
		case TilemapAutotileFormat::Blob47:	   return ptgn::editor::PaintAutotileFormat::Blob47;
		case TilemapAutotileFormat::Subset16:  return ptgn::editor::PaintAutotileFormat::Subset16;
		case TilemapAutotileFormat::DualGrid15:
			return ptgn::editor::PaintAutotileFormat::DualGrid16;
	}
	return ptgn::editor::PaintAutotileFormat::Classic15;
}
#endif

void LoadAssets(Scene& scene) {
	for (const DemoTerrain& terrain : kTerrain) {
		scene.ctx().asset.Load(terrain.texture, terrain.file);
	}
	scene.ctx().asset.Load("regular16_16x16", "assets/autotiles/regular16_16x16.png");
}

void SeedSample(Tilemap map, const TilemapAutotileStyle& style) {
	std::vector<TilemapAutotileStyle> styles{ style };
	for (int y{}; y < 10; ++y) {
		for (int x{}; x < 12; ++x) {
			bool in_patch{ x > 0 && x < 11 && y > 0 && y < 9 };
			bool hole{ x >= 4 && x <= 6 && y >= 3 && y <= 5 };
			bool notch{ y == 2 && x >= 7 && x <= 9 };
			if (in_patch && !hole && !notch) {
				PaintAutotile(map, { x, y }, style.id, styles);
			}
		}
	}
}

void SeedTerrain(Scene& scene) {
	auto& layers{ scene.GetLayers() };
	for (const DemoTerrain& terrain : kTerrain) {
		SceneLayerId layer{ layers.Create(SceneLayerKind::Tile, std::string{ terrain.name }) };
		Tilemap map{ CreateTilemap(scene, layer, Tag{ std::string{ terrain.name } }) };
		if (!map) {
			continue;
		}
		map.SetCellSize({ 16.0f, 16.0f });
		SetPosition(map, terrain.position);
		SeedSample(
			map, TilemapAutotileStyle{
					 .id		   = 1,
					 .format	   = terrain.format,
					 .texture	   = TextureKey{ std::string{ terrain.texture } },
					 .texture_size = terrain.texture_size,
					 .tile_size	   = { 16, 16 },
				 }
		);
	}
	[[maybe_unused]] SceneLayerId paint_layer{ layers.Create(SceneLayerKind::Tile, "Paint Here") };
}

} // namespace

class TilesetPaintingScene : public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);
		SetBackgroundColor(Color{ 25, 29, 36, 255 });
		SeedTerrain(*this);
	}

	void OnLoad() override {
		LoadAssets(*this);
	}
};

PTGN_REGISTER_SCENE(TilesetPaintingScene);

int main(int, char**) {
	Application app{ "Tileset Painting Demo" };
#ifdef PTGN_EDITOR
	auto& editor{ PTGN_WITH_EDITOR(app, true) };
	for (const DemoTerrain& terrain : kTerrain) {
		// Loading a PNG alone does not mark it as an autotile in the editor library.
		// Register the atlas layout before startup, so it is saved in .ptgneditor.
		editor.GetPaintEditor().RegisterAutotileTileset(
			TextureKey{ std::string{ terrain.texture } }, ToEditorFormat(terrain.format),
			{ 16, 16 }, std::string{ terrain.name }
		);
	}
#else
	PTGN_WITH_EDITOR(app, true);
#endif
	app.StartProject<TilesetPaintingScene>("TilesetPaintingProject/TilesetPainting.ptgnproj");
}
