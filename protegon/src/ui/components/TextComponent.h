#pragma once

#include <string>

#include "renderer/FontManager.h"

struct TextComponent {
	TextComponent(const char* content, const engine::Color& color, int size, const char* font_path) : content{ content }, color{ color }, size{ size } {
		engine::FontManager::Load(content, color, size, font_path);
	}
	engine::Color color;
	const char* content;
	int size;
};