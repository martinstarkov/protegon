#pragma once

#include <vector>

#include "ecs/ECS.h"
#include "procedural/Chunk.h"
#include "math/Noise.h"
#include "math/Vector2.h"
#include "renderer/particles/ParticleManager.h"
#include "physics/Collision.h"

namespace engine {

class ChunkManager {
public:
	ChunkManager(const V2_int& tile_size, const V2_int& tiles_per_chunk);
	void Update();
	void Clear();
	void Render();
	void Reset();
private:
	Particle test_particle{ {}, {}, {}, engine::ORANGE, engine::GREY, 10, 0, 0, 0, 0.15 };
	ParticleManager particles{ 300 };

	void mine(ecs::Entity& entity, Collision& collision);
	V2_int tile_size_;
	V2_int tiles_per_chunk_;
	ValueNoise<float> noise;
	std::vector<Chunk*> world_chunks_;
	std::vector<Chunk*> player_chunks_;
};

} // namespace engine