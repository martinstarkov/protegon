#pragma once

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "managers/ResourceManager.h"
#include "renderer/Renderer.h"

struct SDL_Window;

namespace ptgn {

namespace internal {

class Window {
public:
	Window() = delete;
	/*
	* @param window_key Unique identifier for the window.
	* @param window_title Window title.
	* @param window_size Size of window.
	* @param window_position Position of window.
	* @param window_flags Any additional window flags (SDL).
	*/
	Window(const managers::id window_key, const char* window_title, const V2_int& window_size, const V2_int& window_position, std::uint32_t window_flags = 0);
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
	managers::id window_key_;
	Color color{ color::WHITE };
	SDL_Window* window_;
};

} // namespace internal

} // namespace ptgn