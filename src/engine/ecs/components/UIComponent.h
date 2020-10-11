#pragma once

#include <utility>

#include <engine/ui/UIElement.h>

struct UIComponent {
	UIComponent(engine::UIElement element) : element{ std::move(element) } {}
	engine::UIElement element;
};