#pragma once

#include <utility>

#include <engine/ui/UIElement.h>

#include <engine/event/EventHandler.h>

struct UIComponent {
	UIComponent(engine::UIElement element, ecs::Manager* manager = nullptr) : element{ std::move(element) }, manager{ manager } {}
	engine::UIElement element;
	ecs::Manager* manager;
};