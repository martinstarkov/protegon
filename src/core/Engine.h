#pragma once

#include <cstdlib> // std::size_t

#include "math/Vector2.h"
#include "utility/Time.h"

namespace ptgn {

class Engine {
public:
	Engine();
	virtual ~Engine() = default;
	// Starts the engine by creating a window and initiating required systems and the game loop.
	virtual void Start(const char* window_title, const V2_int& window_size, bool window_centered = true, V2_int& window_position = V2_int{}) final;
	virtual void Stop() final;
	// User function called before entering game loop.
	virtual void Init() {}
	// User function called at the beginning of each game frame.
	// dt is the time since the last update (in seconds).
	virtual void Update(double dt) {}
private:
	// Called when engine is first started, sets up all required systems and variables.
	void InternalInit() {
		start = std::chrono::system_clock::now();
		end = std::chrono::system_clock::now();

		Init();
		InternalUpdate();
	}

	// Called when engine is started, contains main game loop and calls user update inside.
	void InternalUpdate();

	using time = std::chrono::time_point<std::chrono::system_clock>;
	
	time start;
	time end;
};

} // namespace ptgn