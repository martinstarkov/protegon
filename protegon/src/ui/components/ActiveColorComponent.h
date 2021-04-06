#pragma once

#include "renderer/Color.h"

struct ActiveColorComponent {
	ActiveColorComponent(const engine::Color& color) : color{ color } {}
	engine::Color color;
};