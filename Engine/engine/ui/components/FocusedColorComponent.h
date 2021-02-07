#pragma once

#include "renderer/Color.h"

struct FocusedColorComponent {
	FocusedColorComponent(const engine::Color& color) : color{ color } {}
	engine::Color color;
};