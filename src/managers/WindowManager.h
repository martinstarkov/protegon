#pragma once

#include "managers/Manager.h"
#include "window/Window.h"

namespace ptgn {

namespace internal {

namespace managers {

class WindowManager : public Manager<Window> {
public:
	WindowManager();
};

} // namespace managers

managers::WindowManager& GetWindowManager();

} // namespace internal

} // namespace ptgn