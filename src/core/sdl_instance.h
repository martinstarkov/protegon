#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"

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
	void SetScale(const V2_float& new_scale);
	V2_float GetScale() const;
private:
	void InitSDL();
	void InitSDLImage();
	void InitSDLTTF();
	void InitSDLMixer();
	void InitWindow();
	void InitRenderer();

	V2_float scale_{ 1.0f, 1.0f };
	Color window_bg_color_{ color::WHITE };
	// TODO: Consider making these shared ptrs to ensure no destruction before use ends?
	SDL_Window* window_{ nullptr };
	SDL_Renderer* renderer_{ nullptr };
};

} // namespace ptgn