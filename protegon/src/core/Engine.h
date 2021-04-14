#pragma once

#include <cstdlib> // std::size_t
#include <cstdint> // std::int32_t, etc
#include <cassert> // assert

#include "ecs/ECS.h"

#include "renderer/Window.h"
#include "renderer/Renderer.h"

#include "debugging/Debug.h"
#include "math/Vector2.h"

#include "utils/Timer.h"

namespace engine {

namespace internal {

// Default window title.
constexpr const char* WINDOW_TITLE{ "Unspecified Window Title" };
// Default window position centered.
constexpr int CENTERED{ 0x2FFF0000u | 0 }; // Equivalent to SDL_WINDOWPOS_CENTERED.
// Default window width.
constexpr int WINDOW_WIDTH{ 600 };
// Default window height.
constexpr int WINDOW_HEIGHT{ 480 };
// Default FPS.
constexpr std::size_t FPS{ 60 };

} // namespace internal

class Engine {
public:
	// Function which is called upon starting the engine.
	virtual void Init() {}
	// Function which is called each frame of the engine loop.
	virtual void Update() {}
	// Function which is called after the Update function, each frame of the engine loop.
	virtual void Render() {}

	template <typename T>
	static void Start(const char* title = internal::WINDOW_TITLE, int width = internal::WINDOW_WIDTH, int height = internal::WINDOW_HEIGHT, std::size_t fps = internal::FPS, int x = internal::CENTERED, int y = internal::CENTERED, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0) {
		Timer timer;
		// Timer started before construction of engine object as that may take some time.
		timer.Start();
		instance_ = new T{};
		auto& engine{ GetInstance() };
		engine.timer_ = timer;
		engine.window_size_ = { width, height };
		engine.window_position_ = { x, y };
		engine.fps_ = fps;
		// Can be useful to cache this as it is often used for conversions.
		engine.inverse_fps_ = 1.0 / static_cast<double>(engine.fps_);
		engine.window_title_ = title;
		engine.running_ = true;
		engine.InitSDL(window_flags, renderer_flags);
		engine.Init();
		engine.Loop();
		engine.Clean();
	}
	static std::pair<Window, Renderer> GenerateWindow(const char* window_title, const V2_int& window_position, const V2_int& window_size, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0);
	static void Delay(std::int64_t milliseconds);
	static void Quit();

	static Window& GetWindow();
	static Renderer& GetRenderer();
	static Renderer& GetRenderer(std::size_t index);
	static V2_int GetScreenSize();
	static int GetScreenWidth();
	static int GetScreenHeight();
	static std::size_t GetFPS();
	static double GetInverseFPS();
private:
	static Engine& GetInstance();
	// Initialize SDL and its sub systems.
	void InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags);
	void Loop();
	void Clean();
	// Unit: milliseconds.
	std::int64_t GetTimeSinceStart() const;
	
	// Class variables.

	static Engine* instance_;
	Window window_{ nullptr };
	Renderer renderer_{ nullptr };
	bool running_{ false };
	// Application run timer.
	Timer timer_;
	V2_int window_size_{ internal::WINDOW_WIDTH, internal::WINDOW_HEIGHT };
	V2_int window_position_{ internal::CENTERED, internal::CENTERED };
	// SDL initialization status, used if multiple windows are created.
	// 0 == initialized succesfully, 1 == not initialized.
	int sdl_init_{ 1 };
	// True type font initialization status, used if multiple windows are created.
	// 0 == initialized succesfully, 1 == not initialized.
	int ttf_init_{ 1 };
	const char* window_title_{ internal::WINDOW_TITLE };
	std::size_t fps_{ internal::FPS };
	double inverse_fps_{ 1.0 / static_cast<double>(internal::FPS) };
};

} // namespace engine
