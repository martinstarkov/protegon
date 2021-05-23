#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "utils/Singleton.h"

struct SDL_Window;

namespace engine {

class Window : public Singleton<Window> {
public:
	/*
	* @return Size of the application window.
	*/
	static V2_int GetSize();

	/*
	* @return Coordinate of the window origin (top left). Not relative to monitor.
	*/
	static V2_int GetOriginPosition();

	/*
	* @return Title of the application window.
	*/
	static const char* GetTitle();

	/*
	* Changes the size of the application window.
	* @param new_size The desired size.
	*/
	static void SetSize(const V2_int& new_size);

	/*
	* Changes the relative origin of the top left of the application window.
	* @param new_origin Vector which will be considered top left position of fwindow.
	*/
	static void SetOriginPosition(const V2_int& new_origin);

	/*
	* Sets the application window title.
	* @param new_title The desired title.
	*/
	static void SetTitle(const char* new_title);

	/*
	* Makes the application window full screen.
	* @param on True for fullscreen, false for windowed.
	*/
	static void SetFullscreen(bool on);
	
	/*
	* Makes the application window resizeable.
	* @param on True for resizeable, false for fixed size.
	*/
	static void SetResizeable(bool on);

private:
	friend class Engine;
	friend class Renderer;
	friend class Singleton<Window>;

	/*
	* Initializes the singleton window instance.
	* @param title Application title.
	* @param position Position of origin for window top left.
	* @param size Desired size of application window.
	* @param flags Flags for creating the window.
	* @return Window singleton instance.
	*/
	static Window& Init(const char* title, 
						const V2_int& position, 
						const V2_int& size, 
						std::uint32_t flags = 0);
	
	// Frees memory used by SDL_Window.
	static void Destroy();

	Window() = default;

	// Conversions to SDL_Window for internal functions.

	operator SDL_Window* () const;
	SDL_Window* operator&() const;

	/*
	* @return True if SDL_Window is not nullptr, false otherwise.
	*/
	bool IsValid() const;

	SDL_Window* window_{ nullptr };
};

} // namespace engine