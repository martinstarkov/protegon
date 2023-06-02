#pragma once

#include "vector2.h"

namespace ptgn {

class Engine {
public:
	Engine();
	~Engine();
	void Construct(const char* window_title,
				   const V2_int& window_size);
	// Override function for when engine is created.
	// Usually this is where one would load fonts and scenes into the respective resource managers.
	virtual void Create();
	virtual void Update(float dt);
private:
	void Loop();
};

inline void Engine::Create() {}

inline void Engine::Update(float dt) {}

} // namespace ptgn