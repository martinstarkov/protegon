#pragma once

#include "vector2.h"

namespace ptgn {

class Engine {
public:
	void Construct(const V2_int& window_size);
	virtual void Create();
	virtual void Update(float dt);
private:

};

inline void Engine::Create() {}

inline void Engine::Update(float dt) {}

} // namespace ptgn