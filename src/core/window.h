#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"

struct SDL_Window;

namespace ptgn {

namespace impl {

class GameInstance;

struct WindowDeleter {
	void operator()(SDL_Window* window);
};

} // namespace impl

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

class Window {
public:
	// Setting fullscreen to true invalidates borderless and resizeable.
	// Setting borderless to true invalidates resizeable.
	void SetupSize(
		const V2_int& resolution, const V2_int& minimum_resolution, bool fullscreen = false,
		bool borderless = false, bool resizeable = true, const V2_float& scale = { 1.0f, 1.0f }
	);

	[[nodiscard]] bool Exists();

	void SetMinimumSize(const V2_int& minimum_size);
	[[nodiscard]] V2_int GetMinimumSize();

	void SetSize(const V2_int& new_size, bool centered = true);
	[[nodiscard]] V2_int GetSize();

	[[nodiscard]] V2_int GetOriginPosition();

	void SetTitle(const char* new_title);
	[[nodiscard]] const char* GetTitle();

	void Center();

	void SetPosition(const V2_int& new_origin);

	void SetFullscreen(bool on);

	// Note: The effect of Maximimize() is cancelled after calling
	// SetResizeable(true).
	void SetResizeable(bool on);

	void SetBorderless(bool on);

	// Note: The effect of Maximimize() is cancelled after calling
	// SetResizeable(true).
	void Maximize();

	void Minimize();

	void Show();

	void Hide();

private:
	friend class Renderer;
	friend class Game;
	friend class impl::GameInstance;
	void Init();
	void SwapBuffers();

	std::unique_ptr<SDL_Window, impl::WindowDeleter> window_;
};

} // namespace ptgn