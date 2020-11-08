#pragma once

#include "renderer/Color.h"

struct HoverColorComponent {
	HoverColorComponent(const engine::Color& color) : color{ color } {}
	engine::Color color;
};