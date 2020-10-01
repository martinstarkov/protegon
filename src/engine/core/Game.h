#pragma once

#include "TextureManager.h"
#include "InputHandler.h"

namespace engine {

namespace internal {

#define CENTERED SDL_WINDOWPOS_CENTERED

// Default window title
constexpr const char* WINDOW_TITLE = "Unknown Title";
// Default window position centered
constexpr int WINDOW_X = CENTERED;
constexpr int WINDOW_Y = CENTERED;
// Default window width
constexpr int WINDOW_WIDTH = 600;
// Default window height
constexpr int WINDOW_HEIGHT = 480;
// Default frame rate
constexpr int FRAME_RATE = 60;

} // namespace internal

class BaseGame {
public:
	virtual void Init() = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
};

class Game : public BaseGame {
public:
	virtual void Init() override {

	}
	virtual void Update() override {

	}
	virtual void Render() override {

	}
protected:
	ecs::Manager manager;
private:
};

class Engine {
public:
	template <typename T> // static assert is base of
	static void Start() {
		Init();
		Loop();
		Clean();
	}
	// Default values defined in engine
	static void Init(const char* title = internal::WINDOW_TITLE, int width = internal::WINDOW_WIDTH, int height = internal::WINDOW_HEIGHT, int frame_rate = internal::FRAME_RATE, int x = internal::WINDOW_X, int y = internal::WINDOW_Y, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0);
	static void Clean();
	static void Quit();
	static void Loop() {
		const std::uint32_t delay = 1000 / frame_rate;
		std::uint32_t start;
		std::uint32_t time;
		while (running) {
			start = SDL_GetTicks();
			// Possibly pass delta T to lambda
			Update();
			Render();
			time = SDL_GetTicks() - start;
			if (delay > time) { // cap frame time at an FPS
				SDL_Delay(delay - time);
			}
		}
	}
	static SDL_Window& GetWindow();
	static SDL_Renderer& GetRenderer();
	static int ScreenWidth();
	static int ScreenHeight();
	static int FPS();
private:
	static void Update() {
		InputHandler::Update();
	}
	static void Render() {
		SDL_RenderClear(renderer);
		TextureManager::SetDrawColor(DEFAULT_RENDERER_COLOR);
		// Render everything here

		SDL_RenderPresent(renderer);

	}
	static void InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags);
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static BaseGame* game;
	static bool running;
	static int window_width;
	static int window_height;
	static int window_x;
	static int window_y;
	static int frame_rate;
	static const char* window_title;
};

} // namespace engine