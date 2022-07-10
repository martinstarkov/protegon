#pragma once

#include "math/Vector2.h"
#include "renderer/Colors.h"

struct SDL_Window;

namespace ptgn {

class Window {
public:
	Window() = delete;
	~Window() = delete;
	/*
	* @param window_title Window title.
	* @param window_size Size of window.
	* @param window_position Position of window.
	* @param window_flags Any additional window flags (SDL).
	*/
	static void Create(const char* window_title, const V2_int& window_size, 
					   const V2_int& window_position = {}, std::uint32_t window_flags = 0);
	static void Destroy();
	static SDL_Window* Get() { return window_; }
	static bool IsValid() { return window_ != nullptr; }
	static V2_int GetSize();
	static V2_int GetOriginPosition();
	static const char* GetTitle();
	static Color GetColor() { return window_color_; }
	static void SetSize(const V2_int& new_size);
	static void SetOriginPosition(const V2_int& new_origin);
	static void SetTitle(const char* new_title);
	static void SetFullscreen(bool on);
	static void SetResizeable(bool on);
	static void SetColor(const Color& new_color) { window_color_ = new_color; }
private:
	static Color window_color_;
	static SDL_Window* window_;
};

} // namespace ptgn