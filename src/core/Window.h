#pragma once

#include "math/Vector2.h"
#include "renderer/Color.h"
#include "core/WindowFlags.h"

namespace ptgn {

namespace window {

/*
* @param window_title Window title.
* @param window_size Size of window.
* @param window_position Position of window.
* @param window_flags Any additional window flags (SDL).
*/
void Init(const char* window_title, const V2_int& window_size, const V2_int& window_position = {}, Flags window_flags = Flags::NONE);
void Release();
bool Exists();
V2_int GetSize();
V2_int GetOriginPosition();
const char* GetTitle();
Color GetColor();
void SetSize(const V2_int& new_size);
void SetOriginPosition(const V2_int& new_origin);
void SetTitle(const char* new_title);
void SetFullscreen(bool on);
void SetResizeable(bool on);
void SetColor(const Color& new_color);
void Show();
void Hide();

} // namespace window

} // namespace ptgn