#pragma once

#include <unordered_map> // std::unordered_map

#include "core/ECS.h"
#include "math/Vector2.h"
#include "world/Chunk.h"

namespace ptgn {

class ChunkManager {
public:
	ChunkManager() = delete;
	~ChunkManager();
	ChunkManager(const V2_int& tiles_per_chunk, const V2_int& tile_size);
	void CenterOn(const V2_int& position, const V2_int& view_distance);
	void Update();
	void Render();
private:

	V2_int tiles_per_chunk_;
	V2_int tile_size_;
	V2_double chunk_size_;

	V2_int coordinate_;
	V2_int size_;

	std::unordered_map<V2_int, Chunk*> loaded_chunks_;
};

} // namespace ptgn