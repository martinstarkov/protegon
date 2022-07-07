#pragma once

#include "managers/SDLManager.h"
#include "core/Window.h"

namespace ptgn {

namespace managers {

class WindowManager : public SDLManager<Window> {
public:
	id GetFirstTargetWindow();
	const id GetTargetWindowId() const;
	void SetTargetWindow(const id window);
private:
	id target_window_{ 0 };
};

} // namespace managers

} // namespace ptgn