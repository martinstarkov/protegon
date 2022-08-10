#pragma once

#include <cstdlib> // std::size_t

#include "math/Vector2.h"
#include "utility/Time.h"
#include "core/WindowFlags.h"

namespace ptgn {

class Engine {
public:
	Engine();
	virtual ~Engine() = default;
	// Starts the engine by creating a window and initiating required systems and the game loop.
	virtual void Start(const char* window_title = "Default Title",
					   const V2_int& window_size = V2_int{},
					   bool window_centered = false, 
					   V2_int& window_position = V2_int{},
					   window::Flags fullscreen_flag = window::Flags::NONE,
					   bool resizeable = true,
					   bool maximize = false) final;
	virtual void Stop() final;
	// User function called before entering game loop.
	virtual void Init() {}
	// User function called at the beginning of each game frame.
	// dt is the time since the last update (in seconds).
	virtual void Update(float dt) {}
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