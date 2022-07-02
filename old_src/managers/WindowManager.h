#pragma once

#include "managers/SDLManager.h"
#include "window/Window.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace internal {

namespace managers {

class WindowManager : public SDLManager<Window> {
public:
	const Window& GetTargetWindow() const;
	Window& GetTargetWindow();

	// This function probably only exists because 
	// I'm missing some obviously better way to do this..
	id GetFirstTargetWindow();
	const id GetTargetWindowId() const;

	void SetTargetWindow(id window);

	const Renderer& GetTargetRenderer() const;
	Renderer& GetTargetRenderer();
private:
	id target_window_{ 0 };
};

class RendererManager : public SDLManager<Renderer> {};

} // namespace managers

} // namespace internal

} // namespace ptgn