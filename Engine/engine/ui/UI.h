#pragma once

#include "ecs/ECS.h"
#include "ecs/Components.h"

#include "statemachine/StateMachineComponent.h"
#include "statemachine/StateMachines.h"

#include "UIComponents.h"
#include "UISystem.h"

#include "math/Vector2.h"

#include "core/Engine.h"

namespace engine {

class UI {
public:
	template <typename T>
	static ecs::Entity AddButton(ecs::Manager& ui_manager, Scene& scene, const V2_int& position, const V2_int& size, const Color& background_color) {
		auto entity = ui_manager.CreateEntity();
		engine::EventHandler::Register<T>(entity);
		entity.AddComponent<EventComponent>(scene);
		auto& sm = entity.AddComponent<StateMachineComponent>();
		sm.AddStateMachine<ButtonStateMachine>("button", entity);
		entity.AddComponent<TransformComponent>(position);
		entity.AddComponent<SizeComponent>(size);
		entity.AddComponent<BackgroundColorComponent>(background_color);
		entity.AddComponent<RenderComponent>();
		return entity;
	}
	static ecs::Entity AddText(ecs::Manager& ui_manager, const V2_int& position, const V2_int& size, const Color& background_color) {
		auto entity = ui_manager.CreateEntity();
		entity.AddComponent<TransformComponent>(position);
		entity.AddComponent<SizeComponent>(size);
		entity.AddComponent<BackgroundColorComponent>(background_color);
		entity.AddComponent<RenderComponent>();
		return entity;
	}
private:

};

} // namespace engine