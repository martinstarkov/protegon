#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"
#include "protegon/texture.h"

struct SDL_Window;
struct SDL_Renderer;

namespace ptgn {

class SDLInstance {
public:
	SDLInstance();
	~SDLInstance();

	std::shared_ptr<SDL_Window> GetWindow() const;
	std::shared_ptr<SDL_Renderer> GetRenderer() const;

	void SetWindowBackgroundColor(const Color& new_color);
	Color GetWindowBackgroundColor() const;

	void SetScale(const V2_float& new_scale);
	V2_float GetScale() const;

	void SetResolution(const V2_int& new_resolution);
	V2_int GetResolution() const;

	bool WindowExists() const;

	void SetDrawMode(const Color& color, Texture::DrawMode draw_mode);
private:
	void InitSDL();
	void InitSDLImage();
	void InitSDLTTF();
	void InitSDLMixer();
	void InitWindow();
	void InitRenderer();

	V2_float scale_{ 1.0f, 1.0f };
	V2_int resolution_{ 1280, 720 };

	Color window_bg_color_{ color::WHITE };
	
	std::shared_ptr<SDL_Window> window_;
	std::shared_ptr<SDL_Renderer> renderer_;
};

namespace impl {

std::shared_ptr<SDL_Renderer> SetDrawColor(const Color& color);

} // namespace impl

} // namespace ptgn