#pragma once

#include <utility>

#include "ecs/ECS.h"

#include "event/EventHandler.h"

struct UIComponent {
	UIComponent(const ecs::Entity& entity) : entity{ entity } {}
	template <typename T>
	void AddEvent() {
		engine::EventHandler::Register<T>(entity);
	}
	~UIComponent() {
		engine::EventHandler::Remove(entity);
	}
	ecs::Entity entity;
};