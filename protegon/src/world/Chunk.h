#pragma once

#include <vector> // std::vector

#include "core/ECS.h"
#include "math/Vector2.h"

namespace ptgn {

class Chunk {
public:
	virtual ~Chunk() = default;
	virtual void Create(const V2_int& coordinate, const V2_int& tiles, const V2_int& tile_size) {}
	virtual void Render() {}
	friend bool operator==(const Chunk& a, const Chunk& b) {
		return a.coordinate_ == b.coordinate_;
	}
	V2_int coordinate_;
protected:
	ecs::Manager manager_;
private:
	friend class ChunkManager;
	bool flagged_{ false };
};

class BasicChunk : public Chunk {
public:
	virtual ~BasicChunk() = default;
	virtual void Create(const V2_int& position, const V2_int& tiles, const V2_int& tile_size) override final;
	virtual void Render() override final;
};

} // namespace ptgn