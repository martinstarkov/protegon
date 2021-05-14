#pragma once

#include <cstdlib> // std::size_t
#include <cstdint> // std::int32_t, etc
#include <cassert> // assert
#include <tuple> // std::pair
#include <unordered_map> // std::unordered_map

#include "ecs/ECS.h"

#include "renderer/Window.h"
#include "renderer/Renderer.h"

#include "debugging/Debug.h"
#include "math/Vector2.h"

#include "utils/Timer.h"
#include "utils/TypeTraits.h"

namespace engine {

// first - Window object.
// second - Associated Renderer object.
using Display = std::pair<Window, Renderer>;

namespace internal {

// Definition for a screen position that is centered on the user's monitor.
inline const V2_int CENTERED{ 0x2FFF0000u | 0, 0x2FFF0000u | 0 }; // Equivalent to SDL_WINDOWPOS_CENTERED in X and Y.

} // namespace internal

class Engine {
public:
	// Function which is called upon starting the engine.
	virtual void Init() {}
	// Function which is called each frame of the engine loop.
	virtual void Update() {}
	// Function which is called after the Update function, each frame of the engine loop.
	virtual void Render() {}

	// Maximum frames per second of the engine loop.
	// Optional: Cast to desired type.
	template <typename T = double,
		type_traits::is_number<T> = true>
	static T GetFPS() {
		auto& engine{ GetInstance() };
		return static_cast<T>(engine.fps_);
	}

	// Start the engine application.
	// Creates the primary window and renderer.
	template <typename T,
		type_traits::is_base_of<Engine, T> = true,
		type_traits::is_default_constructible<T> = true>
	static void Start(const char* window_title, 
					  const V2_int& window_size, 
					  std::size_t frames_per_second, 
					  const V2_int& window_position = internal::CENTERED, 
					  std::uint32_t window_flags = 0, 
					  std::uint32_t renderer_flags = 0) {
		// Create an instance of the derived engine implementation.
		instance_ = new T{};

		auto& engine{ GetInstance() };
		// Start the application timer.
		engine.timer_.Start();
		engine.InitSDLComponents();
		engine.InitInternals();
		// Create the primary window and renderer (display).
		auto display{ CreateDisplay(window_title, window_position, window_size, window_flags, renderer_flags) };
		// Set display index to match that of the primary window.
		engine.primary_display_index_ = display.first.GetDisplayIndex();

		engine.fps_ = frames_per_second;
		engine.running_ = true;
		
		engine.Init();
		engine.Loop();
		engine.Clean();
	}
	// Returns a display std::pair (first=Window, second=Renderer).
	// Displays are indexed based on order of creation.
	// I.e. initial display has index 0, second created display has index 1, etc.
	static Display CreateDisplay(const char* window_title, 
								 const V2_int& window_position, 
								 const V2_int& window_size, 
								 std::uint32_t window_flags = 0, 
								 std::uint32_t renderer_flags = 0);

	static Display GetDisplay(std::size_t display_index = 0);

	static void SetDefaultDisplay(std::size_t display_index = 0);
	static void SetDefaultDisplay(const Window& window);
	static void SetDefaultDisplay(const Renderer& renderer);

	static void DestroyDisplay(std::size_t display_index = 0);
	static void DestroyDisplay(const Window& window);
	static void DestroyDisplay(const Renderer& renderer);

	static void Quit();

	static void Delay(std::int64_t milliseconds);
private:
	friend class InputHandler;
	friend class Renderer;
	static Engine* instance_;
	static Engine& GetInstance();
	static void Quit(const Window& window);
	static std::size_t GetPrimaryDisplayIndex();

	void InitSDLComponents();
	void InitInternals();
	void Loop();
	void Clean();

	// Unit: milliseconds.
	std::int64_t GetTimeSinceStart() const;
	
	// Class variables.

	// Key: display_index associated with a given Display.
	// Value: Display object (std::pair containing Window and Renderer respectively).
	std::unordered_map<std::size_t, Display> display_map_;
	// First valid display is 1 (pre-incremented).
	std::size_t next_display_index_{ 0 };
	// Default display index is the index of the display that is considered the primary one.
	std::size_t primary_display_index_{ 0 };

	std::size_t fps_{ 60 };
	bool running_{ false };
	
	// Application run timer.
	Timer timer_;
	
	// SDL subsystems' status, true if all initialized successfully, false otherwise.
	bool init_{ false };
};

} // namespace engine
