#include "Chunk.h"

#include "components/TransformComponent.h"
#include "components/ShapeComponent.h"
#include "components/Tags.h"
#include "systems/DrawShapeSystem.h"
#include "renderer/WorldRenderer.h"

namespace ptgn {

void BasicChunk::Create(const V2_int& position, const V2_int& tiles, const V2_int& tile_size) {
	position_ = position;
	AABB tile{ tile_size };
	manager_.Reserve(tiles.x * tiles.y);
	for (auto i{ 0 }; i < tiles.x; ++i) {
		for (auto j{ 0 }; j < tiles.y; ++j) {
			auto coordinate = position + V2_int{ i, j } * tile_size;
			auto entity = manager_.CreateEntity();
			entity.AddComponent<TransformComponent>(Transform{ coordinate });
			entity.AddComponent<ShapeComponent>(tile);
			entity.AddComponent<RenderComponent>();
			entity.AddComponent<ColorComponent>(Color::RandomSolid());
		}
	}
	manager_.Refresh();
}

void BasicChunk::Render() {
	manager_.ForEachEntityWith<TransformComponent, ShapeComponent, RenderComponent>(DrawShapeSystem<WorldRenderer>{});
}

} // namespace ptgn