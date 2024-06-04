#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace screen {

V2_int GetSize();

} // namespace screen

namespace window {

// Setting fullscreen to true invalidates borderless and resizeable.
// Setting borderless to true invalidates resizeable.
void SetupSize(const V2_int& resolution, const V2_int& minimum_resolution, bool fullscreen = false, bool borderless = false, bool resizeable = true, const V2_float& scale = { 1.0f, 1.0f });

void SetScale(const V2_float& scale);
V2_float GetScale();

bool Exists();

void Clear();

void SetResolution(const V2_int& resolution);
V2_int GetResolution();

void SetMinimumSize(const V2_int& minimum_size);
V2_int GetMinimumSize();

void SetSize(const V2_int& new_size, bool centered = true);
V2_int GetSize();

V2_int GetOriginPosition();

void SetTitle(const char* new_title);
const char* GetTitle();

Color GetColor();
void SetColor(const Color& new_color);

void Center();

void SetPosition(const V2_int& new_origin);

void SetFullscreen(bool on);

// Note: The effect of Maximimize() is cancelled after calling SetResizeable(true).
void SetResizeable(bool on);

void SetBorderless(bool on);

// Note: The effect of Maximimize() is cancelled after calling SetResizeable(true).
void Maximize();

void Minimize();

void Show();

void Hide();

} // namespace window

} // namespace ptgn