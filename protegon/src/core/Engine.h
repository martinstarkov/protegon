#pragma once

#include <cstdlib> // std::size_t

#include "core/SceneManager.h"
#include "math/Vector2.h"
#include "utils/Singleton.h"
#include "utils/Timer.h"
#include "utils/TypeTraits.h"

namespace ptgn {

namespace internal {

// Definition for a screen position that is centered on the user's monitor.
// Equivalent to SDL_WINDOWPOS_CENTERED in X and Y.
inline const V2_int CENTERED{ 0x2FFF0000u | 0, 0x2FFF0000u | 0 };

// Window flag which allow for resizing.
inline const int RESIZEABLE{ 32 };

} // namespace internal

class Scene;

class Engine : public Singleton<Engine> {
public:
	// Entering engine execution loop.
	template <typename TInitialScene, 
		type_traits::is_base_of_e<Scene, TInitialScene> = true>
		static void Start(const char* scene_key,
						  const char* window_title,
						  const V2_int& window_size,
						  std::size_t fps,
						  const V2_int& window_position = internal::CENTERED,
						  std::uint32_t window_flags = 0,
						  std::uint32_t renderer_flags = 0) {
		auto& instance{ GetInstance() };
		instance.application_timer_.Start();
		instance.running_ = true;
		SetFPS(fps);
		assert(!instance.init_ && "Cannot start engine before the engine loop has stopped");
		instance.InitSDLComponents();
		CreateDisplay(window_title, window_position, window_size, window_flags, renderer_flags);
		SceneManager::AddScene<TInitialScene>(scene_key);
		SceneManager::SetActiveScene(scene_key);
		instance.Loop();
		instance.Destroy();
	}

	// Pauses engine code execution for the given amount of milliseconds.
	static void Delay(milliseconds duration);

	// Exits the engine loop and frees memory used by engine.
	static void Quit();
private:
	friend class Singleton<Engine>;

	// Defines granularity of time units used in frame rate control.
	using time = nanoseconds;
	
	// Sets engine frame rate and caches inverse fps.
	static void SetFPS(std::size_t fps);

	// Creates the application window.
	static void CreateDisplay(const char* window_title,
							  const V2_int& window_position, 
							  const V2_int& window_size, 
							  std::uint32_t window_flags = 0, 
							  std::uint32_t renderer_flags = 0);

	Engine() = default;
	~Engine() = default;
	
	// Initializes SDL related components.
	void InitSDLComponents();

	// Engine loop function repeated until Quit is called.
	void Loop();

	// Wrapper for all internal engine update / render systems.
	void Update();

	// Clears engine related memory usage.
	void Destroy();

	// Frame rate (per second).
	std::size_t fps_{ 0 };
	// Length of a single frame in time units.
	time frame_time_{ 0 };
	// Used for running the engine loop.
	bool running_{ false };
	// Stores state of engine initialization to prevent secondary Start calls.
	bool init_{ false };
	// Timer for checking time since application start.
	Timer application_timer_;
};

} // namespace ptgn