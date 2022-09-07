#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace window {

bool Exists();

void Clear();

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

void Maximize();

void Minimize();

void Show();

void Hide();

} // namespace window

} // namespace ptgn