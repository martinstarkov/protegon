#pragma once

#include "ecs/ECS.h"
#include "ecs/Components.h"

#include "UIComponents.h"

#include "utils/math/Vector2.h"

#include "core/Engine.h"

namespace engine {

class UI {
public:
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
	static ecs::Entity AddText(ecs::Manager& ui_manager, V2_int position, V2_int size, const Color& background_color) {
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