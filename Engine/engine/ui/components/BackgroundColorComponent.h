#pragma once

#include "renderer/Color.h"

struct BackgroundColorComponent {
	BackgroundColorComponent(const engine::Color& color) : original_color{ color }, color{ color } {}
	engine::Color original_color;
	engine::Color color;
};