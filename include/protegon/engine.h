#pragma once

#include "vector2.h"

namespace ptgn {

class Engine {
public:
	Engine();
	~Engine();
	void Construct(const char* window_title,
				   const V2_int& window_size);
	virtual void Create();
	virtual void Update(float dt);
private:
	void Loop();
};

inline void Engine::Create() {}

inline void Engine::Update(float dt) {}

} // namespace ptgn