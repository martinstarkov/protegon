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
	ChunkManager(const V2_int& tiles_per_chunk,
				 const V2_int& tile_size,
				 const V2_int& load_size,
				 const V2_int& update_size,
				 const V2_int& render_size);
	void CenterOn(const V2_double& position);
	void Update();
	void Render();
	V2_int GetTileSize() const;
	V2_int GetTilesPerChunk() const;
	V2_int GetChunkSize() const;
private:

	V2_int tiles_per_chunk_;
	V2_int tile_size_;
	
	V2_double position_;
	V2_double chunk_size_;

	V2_int load_size_;
	V2_int update_size_;
	V2_int render_size_;

	std::unordered_map<V2_int, Chunk*> loaded_chunks_;
};

} // namespace ptgn