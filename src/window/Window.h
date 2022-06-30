#pragma once

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "renderer/Renderer.h"

struct SDL_Window;

namespace ptgn {

namespace internal {

struct Window {
public:
	Window() = delete;
	/*
	* @param window_title Window title.
	* @param window_size Size of window.
	* @param window_position Position of window.
	* @param window_flags Any additional window flags (SDL).
	*/
	Window(const char* window_title, const V2_int& window_size, const V2_int& window_position, std::uint32_t window_flags = 0);
	~Window();
	bool Exists() const;
	V2_int GetSize() const;
	V2_int GetOriginPosition() const;
	const char* GetTitle() const;
	Color GetColor() const;
	void SetSize(const V2_int& new_size);
	void SetOriginPosition(const V2_int& new_origin);
	void SetTitle(const char* new_title);
	void SetFullscreen(bool on);
	void SetResizeable(bool on);
	void SetColor(const Color& new_color);
	operator SDL_Window*() const;
	const Renderer& GetRenderer() const;
private:
	Color color{ color::WHITE };
	SDL_Window* window_;
	Renderer renderer_;
};

} // namespace internal

} // namespace ptgn