#pragma once

#include <string>

#include <engine/ecs/ECS.h>

#include <engine/renderer/Color.h>

#include <engine/utils/Vector2.h>
#include <engine/renderer/FontManager.h>

namespace engine {

class UIElement {
public:
	UIElement(const char* font_text, const int font_size, const char* font_path, const Color& font_color, const Color& background_color, const Color& hover_color, const Color& active_color, ecs::Manager* influence_manager = nullptr) : font_text{ font_text }, font_color{ font_color }, background_color{ background_color }, original_background_color{ background_color }, hover_color{ hover_color }, active_color{ active_color }, influence_manager{ influence_manager } {
		FontManager::Load(this->font_text, font_color, font_size, font_path);
	}
	virtual ~UIElement() = default;
	void ResetBackgroundColor() { background_color = original_background_color; }
	Color GetBackgroundColor() { return background_color; }
	bool IsActive() { return active; }
	ecs::Manager* GetInfluenceManager() { return influence_manager; }
	std::string GetText() { return font_text; }
	virtual void Hover() {}
	virtual void Activate(V2_double mouse_offset) {}
	virtual bool HasText() { return false; }
	virtual bool HasInvoke() { return false; }
protected:
	ecs::Manager* influence_manager;
	bool active = false;
	V2_double mouse_offset;
	std::string font_text;
	Color font_color;
	Color original_background_color;
	Color background_color;
	Color hover_color;
	Color active_color;
};

class UITextBox : public UIElement {
public:
	UITextBox(const char* font_text, const int font_size, const char* font_path, const Color& font_color, const Color& background_color) : UIElement(font_text, font_size, font_path, font_color, background_color, background_color, background_color) {}
	virtual bool HasText() override { return true; }
};

class UIButton : public UIElement {
public:
	UIButton(const char* font_text, const int font_size, const char* font_path, const Color& font_color, const Color& background_color, const Color& hover_color, const Color& active_color, ecs::Manager* influence_manager) : UIElement(font_text, font_size, font_path, font_color, background_color, hover_color, active_color, influence_manager) {}
	virtual bool HasText() override { return true; }
	virtual bool HasInvoke() { return true; }
	virtual void Hover() override {
		active = false;
		mouse_offset = {};
		background_color = hover_color;
	}
	virtual void Activate(V2_double mouse_offset) override {
		active = true;
		this->mouse_offset = mouse_offset;
		background_color = active_color;
	}
protected:
};

} // namespace engine