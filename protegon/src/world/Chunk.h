#pragma once

#include <vector> // std::vector

#include "core/ECS.h"
#include "math/Vector2.h"


namespace ptgn {

class ChunkManager;

class Chunk {
public:
	virtual ~Chunk() = default;
	virtual void Create() {}
	virtual void Render() {}
	friend bool operator==(const Chunk& a, const Chunk& b) {
		return a.coordinate_ == b.coordinate_;
	}
protected:
	V2_int coordinate_;
	ecs::Manager manager_;
	ChunkManager* parent_{ nullptr };
private:
	void Init(ChunkManager* parent, const V2_int& coordinate) {
		parent_ = parent;
		coordinate_ = coordinate;
	}
	friend class ChunkManager;
	bool flagged_{ false };
};

class BasicChunk : public Chunk {
public:
	virtual ~BasicChunk() = default;
	virtual void Create() override final;
	virtual void Render() override final;
};

} // namespace ptgn