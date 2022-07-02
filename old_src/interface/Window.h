#pragma once

#include "math/Vector2.h"
#include "utils/Defines.h"
#include "renderer/Colors.h"
#include "managers/ResourceManager.h"

namespace ptgn {

namespace window {

/*
* Creates a single application window with a given title, pixel size, and flags.
* If a window has already been created, its title and size will be updated.
* Returns the id of the created window
*/
internal::managers::id Create(const char* window_title, const V2_int& window_size, const V2_int& window_position = CENTERED, std::uint32_t window_flags = 0);

// Set a new default window target.
void SetDefault(internal::managers::id window);

// Closes the currently active application window.
void Destroy(bool default = true, internal::managers::id window = 0);

// Check if a window is currently open.
bool Exists(bool default = true, internal::managers::id window = 0);

/*
* @return Size of the application window.
*/
V2_int GetSize(bool default = true, internal::managers::id window = 0);

/*
* @return Coordinate of the window origin (top left). Not relative to monitor.
*/
V2_int GetOriginPosition(bool default = true, internal::managers::id window = 0);

/*
* @return Title of the application window.
*/
const char* GetTitle(bool default = true, internal::managers::id window = 0);

/*
* @return Background color of the window.
*/
Color GetColor(bool default = true, internal::managers::id window = 0);

/*
* Changes the size of the application window.
* @param new_size The desired size.
*/
void SetSize(const V2_int& new_size, bool default = true, internal::managers::id window = 0);

/*
* Changes the relative origin of the top left of the application window.
* @param new_origin Vector which will be considered top left position of fwindow.
*/
void SetOriginPosition(const V2_int& new_origin, bool default = true, internal::managers::id window = 0);

/*
* Sets the application window title.
* @param new_title The desired title.
*/
void SetTitle(const char* new_title, bool default = true, internal::managers::id window = 0);

/*
* Makes the application window full screen.
* @param state True for fullscreen, false for windowed.
*/
void SetFullscreen(bool state, bool default = true, internal::managers::id window = 0);

/*
* Makes the application window resizeable.
* @param state True for resizeable, false for fixed size.
*/
void SetResizeable(bool state, bool default = true, internal::managers::id window = 0);

/*
* Sets the background color of the window.
* @param color The color to which the window background will be set.
*/
void SetColor(const Color& color = color::WHITE, bool default = true, internal::managers::id window = 0);

} // namespace window

} // namespace ptgn