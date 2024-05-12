#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace window {

bool Exists();

void Clear();

void SetLogicalSize(const V2_int& logical_size);

V2_int GetLogicalSize();

V2_int GetSize();

V2_int GetOriginPosition();

const char* GetTitle();

Color GetColor();

void SetSize(const V2_int& new_size, bool centered = true);

void Center();

void SetPosition(const V2_int& new_origin);

void SetTitle(const char* new_title);

void SetFullscreen(bool on);

// Note: The effect of Maximimize() is cancelled after calling SetResizeable(true).
void SetResizeable(bool on);

void SetColor(const Color& new_color);

// Note: The effect of Maximimize() is cancelled after calling SetResizeable(true).
void Maximize();

void Minimize();

void Show();

void Hide();

void SetScale(const V2_float& new_scale);

V2_float GetScale();

} // namespace window

} // namespace ptgn