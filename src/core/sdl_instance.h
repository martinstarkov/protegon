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

	[[nodiscard]] std::shared_ptr<SDL_Window> GetWindow() const;
	[[nodiscard]] std::shared_ptr<SDL_Renderer> GetRenderer() const;

	void SetWindowBackgroundColor(const Color& new_color);
	[[nodiscard]] Color GetWindowBackgroundColor() const;

	void SetScale(const V2_float& new_scale);
	[[nodiscard]] V2_float GetScale() const;

	void SetResolution(const V2_int& new_resolution);
	[[nodiscard]] V2_int GetResolution() const;

	[[nodiscard]] bool WindowExists() const;

	void SetDrawMode(const Color& color, Texture::BlendMode draw_mode);
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