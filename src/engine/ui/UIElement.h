#pragma once

#include <string>

#include <engine/ecs/ECS.h>

#include <engine/renderer/Color.h>

#include <engine/utils/Vector2.h>
#include <engine/renderer/FontManager.h>

namespace engine {

struct UIElement {
	UIElement(const char* font_text, const int font_size, const char* font_path, const Color& font_color, const Color& background_color, const Color& hover_color, const Color& active_color, ecs::Manager* manager = nullptr) : font_text{ font_text }, font_color{ font_color }, background_color{ background_color }, hover_color{ hover_color }, active_color{ active_color }, manager{ manager } {
		FontManager::Load(this->font_text, font_color, font_size, font_path);
	}
	ecs::Manager* manager;
	bool interacting = false;
	V2_double mouse_offset;
	std::string font_text;
	Color font_color;
	Color background_color;
	Color hover_color;
	Color active_color;
};

} // namespace engine