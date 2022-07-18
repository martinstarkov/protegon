#pragma once

#include <vector> // std::vector

#include "core/ECS.h"
#include "math/Vector2.h"
#include "renderer/Color.h"

namespace ptgn {

class ChunkManager;

class Chunk {
public:
	virtual ~Chunk() = default;

	virtual void Create() {}

	virtual void Update() {}

	virtual void Render() {}

	bool operator==(const Chunk& b) {
		return coordinate_ == b.coordinate_;
	}

	const ecs::Manager& GetManager() const {
		return manager_;
	}

	ecs::Manager& GetManager() {
		return manager_;
	}
protected:
	V2_int coordinate_;

	ecs::Manager manager_;

	ChunkManager* parent_{ nullptr };
private:
	friend class ChunkManager;

	void Init(ChunkManager* parent, const V2_int& coordinate) {
		parent_ = parent;
		coordinate_ = coordinate;
	}

	bool render_{ false };
	bool update_{ false };
};

} // namespace ptgn