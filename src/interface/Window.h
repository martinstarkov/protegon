#pragma once

#include "math/Vector2.h"
#include "utils/Defines.h"

namespace ptgn {

namespace window {

/*
Creates a single application window with a given title, pixel size, and flags.
If a window exists, its title, size, and flags will be modified.
*/
void Create(const char* title, const V2_int& size, const V2_int& position = CENTERED, int flags = 0);

// Closes the currently active application window.
void Destroy();

// Check if a window is currently open.
bool Exists();

/*
* @return Size of the application window.
*/
V2_int GetSize();

/*
* @return Coordinate of the window origin (top left). Not relative to monitor.
*/
V2_int GetOriginPosition();

/*
* @return Title of the application window.
*/
const char* GetTitle();

/*
* Changes the size of the application window.
* @param new_size The desired size.
*/
void SetSize(const V2_int& new_size);

/*
* Changes the relative origin of the top left of the application window.
* @param new_origin Vector which will be considered top left position of fwindow.
*/
void SetOriginPosition(const V2_int& new_origin);

/*
* Sets the application window title.
* @param new_title The desired title.
*/
void SetTitle(const char* new_title);

/*
* Makes the application window full screen.
* @param state True for fullscreen, false for windowed.
*/
void SetFullscreen(bool state);

/*
* Makes the application window resizeable.
* @param state True for resizeable, false for fixed size.
*/
void SetResizeable(bool state);

} // namespace window

} // namespace ptgn