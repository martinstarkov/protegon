#pragma once

#include "ecs/ECS.h"
#include "ecs/Components.h"

#include "UIComponents.h"

#include "ui/UIElement.h"

#include "utils/Vector2.h"

#include "core/Engine.h"

namespace engine {

class UI {
public:
	template <typename T>
	static ecs::Entity AddInteractable(ecs::Manager* ui_manager, V2_int position, V2_int size, UIElement* element) {
		auto ui_entity = AddStatic(ui_manager, position, size, element);
		auto& ui = ui_entity.GetComponent<UIComponent>();
		ui.AddEvent<T>();
		return ui_entity;
	}
	static ecs::Entity AddStatic(ecs::Manager* ui_manager, V2_int position, V2_int size, UIElement* element) {
		assert(ui_manager != nullptr && "Invalid ui_manager pointer");
		auto ui_entity = ui_manager->CreateEntity();
		ui_entity.AddComponent<UIComponent>(element, ui_entity);
		ui_entity.AddComponent<TransformComponent>(position);
		ui_entity.AddComponent<SizeComponent>(size);
		ui_entity.AddComponent<RenderComponent>();
		return ui_entity;
	}
	template <typename T>
	static ecs::Entity AddButton(ecs::Manager& ui_manager, ecs::Manager& influence_manager, V2_int position, V2_int size, const Color& background_color) {
		auto entity = ui_manager.CreateEntity();
		engine::EventHandler::Register<T>(entity);
		entity.AddComponent<EventComponent>();
		entity.AddComponent<InfluenceComponent>(influence_manager);
		entity.AddComponent<StateComponent>();
		entity.AddComponent<TransformComponent>(position);
		entity.AddComponent<SizeComponent>(size);
		entity.AddComponent<BackgroundColorComponent>(background_color);
		entity.AddComponent<RenderComponent>();
		return entity;
	}
private:

};

} // namespace engine