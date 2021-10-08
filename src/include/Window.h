#pragma once

#include "math/Vector2.h"
#include "utils/Defines.h"
#include "renderer/Color.h"

namespace ptgn {

namespace window {

// Presents the drawn objects to the screen. Must be called once drawing is done.
void Present();

// Clear the drawn objects from the screen.
void Clear();

// Sets the background color of the window.
void SetColor(const Color& color);

/*
Creates a single application window with a given title, pixel size, and flags.
If a window exists, its title, size, and flags will be modified.
*/
void Create(const char* title, const V2_int& size, const V2_int& position = CENTERED, int flags = 0);

// Closes the currently active application window.
void Destroy();

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
* @param on True for fullscreen, false for windowed.
*/
void SetFullscreen(bool on);

/*
* Makes the application window resizeable.
* @param on True for resizeable, false for fixed size.
*/
void SetResizeable(bool on);

} // namespace window

} // namespace ptgn