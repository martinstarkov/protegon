#include "Chunk.h"

#include "components/TransformComponent.h"
#include "components/ShapeComponent.h"
#include "components/Tags.h"
#include "systems/DrawShapeSystem.h"
#include "renderer/WorldRenderer.h"
#include "world/LevelManager.h"
#include "world/ChunkManager.h"

// TODO: REMOVE
#include "debugging/DebugRenderer.h"

namespace ptgn {

void BasicChunk::Create() {
	const Level& level = LevelManager::GetLevel("level1");
	auto level_size = level.GetSize();
	auto tiles = parent_->GetTilesPerChunk();
	auto tile_size = parent_->GetTileSize();
	AABB tile{ tile_size };
	//manager_.Reserve(tiles.x * tiles.y);
	for (auto i{ 0 }; i < tiles.x; ++i) {
		for (auto j{ 0 }; j < tiles.y; ++j) {
			auto tile_position = coordinate_ * tiles + V2_int{ i, j };
			Color color = colors::WHITE;
			if (tile_position.x < level_size.x && tile_position.y < level_size.y) {
				color = level.GetColor(tile_position);
			}
			if (color != colors::WHITE) {
				auto position = tile_position * tile_size;
				auto entity = manager_.CreateEntity();
				entity.AddComponent<TransformComponent>(Transform{ position });
				entity.AddComponent<ShapeComponent>(tile);
				entity.AddComponent<RenderComponent>();
				entity.AddComponent<ColorComponent>(color);
			}
		}
	}
	manager_.Refresh();
}

void BasicChunk::Render() {
	manager_.ForEachEntityWith<TransformComponent, ShapeComponent, RenderComponent>(DrawShapeSystem<WorldRenderer>{});
	auto chunk_size = parent_->GetChunkSize();
	DebugRenderer<WorldRenderer>::DrawRectangle(coordinate_ * chunk_size, chunk_size, colors::BLACK);
}

} // namespace ptgn