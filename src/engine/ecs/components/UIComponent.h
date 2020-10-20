#pragma once

#include <utility>

#include <engine/ecs/ECS.h>

#include <engine/event/EventHandler.h>

#include <engine/ui/UIElement.h>

struct UIComponent {
	UIComponent(engine::UIElement* element, const ecs::Entity& entity) : element{ std::move(element) }, entity{ entity } {}
	template <typename T>
	void AddEvent() {
		engine::EventHandler::Register<T>(entity);
	}
	~UIComponent() {
		engine::EventHandler::Remove(entity);
		delete element;
	}
	engine::UIElement* operator->() {
		return element;
	}
	engine::UIElement* element;
	ecs::Entity entity;
};