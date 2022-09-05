#pragma once

#include "protegon/color.h"

struct SDL_Window;
struct SDL_Renderer;

namespace ptgn {

class SDLInstance {
public:
	SDLInstance();
	~SDLInstance();
	SDL_Window* GetWindow() const;
	SDL_Renderer* GetRenderer() const;
	void SetWindowBackgroundColor(const Color& new_color);
	Color GetWindowBackgroundColor() const;
private:
	void InitSDL();
	void InitSDLImage();
	void InitSDLTTF();
	void InitSDLMixer();
	void InitWindow();
	void InitRenderer();

	Color window_bg_color_{ color::WHITE };
	// TODO: Consider making these shared ptrs to ensure no destruction before use ends?
	SDL_Window* window_{ nullptr };
	SDL_Renderer* renderer_{ nullptr };
};

} // namespace ptgn