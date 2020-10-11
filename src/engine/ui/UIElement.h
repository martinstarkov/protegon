#pragma once

#include <string>

#include <engine/renderer/Color.h>

#include <engine/utils/Vector2.h>

namespace engine {

struct UIElement {
	UIElement(const char* text, Color color) : name{ text }, color{ color } {

	}
	bool interacting = false;
	V2_double mouse_offset;
	std::string name;
	Color color;
};

} // namespace engine