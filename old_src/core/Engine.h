#pragma once

#include <chrono>

#include "math/Vector2.h"

namespace ptgn {

class Engine {
public:
	// Starts the engine by creating a window and initiating required systems and the game loop.
	void Start(const char* window_title, const V2_int& window_size);
	// User function called before entering game loop.
	virtual void Init() {}
	// User function called at the beginning of each game frame.
	virtual void Update(double dt) {}
private:
	// Called when engine is first started, sets up all required systems and variables.
	void InternalInit();

	// Called when engine is started, contains main game loop and calls user update inside.
	void InternalUpdate();

	using time = std::chrono::time_point<std::chrono::system_clock>;
	
	time start;
	time end;
};

} // namespace ptgn