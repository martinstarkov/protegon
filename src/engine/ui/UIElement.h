#pragma once

#include <string>

#include <engine/renderer/Color.h>

namespace engine {

struct UIElement {
	UIElement(const char* text, Color color) : name{ text }, color{ color } {

	}
	std::string name;
	Color color;
};

} // namespace engine