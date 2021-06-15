#pragma once

#include <vector> // std::vector

#include "core/ECS.h"
#include "math/Vector2.h"
#include "world/Chunk.h"

namespace ptgn {

class ChunkManager {
public:
	ChunkManager() = delete;
	~ChunkManager();
	ChunkManager(const V2_int& tiles_per_chunk, const V2_int& tile_size);
	void SetRenderRadius(const V2_int& chunk_radius);
	void SetUpdateRadius(const V2_int& chunk_radius);
	void CenterOn(const V2_int& position, const V2_int& chunk_radius);
	void Update();
	void Render();
private:

	V2_int render_size_;
	V2_int update_size_;

	V2_int active_position_;
	V2_int active_size_;

	V2_int previous_position_;
	V2_int previous_size_;

	V2_int tiles_per_chunk_;
	V2_int tile_size_;
	V2_int chunk_size_;

	std::vector<Chunk*> loaded_chunks_;
	std::vector<Chunk*> update_chunks_;
	std::vector<Chunk*> render_chunks_;
};

} // namespace ptgn