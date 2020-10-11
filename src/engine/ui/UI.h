#pragma once

#include <engine/ecs/ECS.h>
#include <engine/ecs/Components.h>

#include <engine/ui/UIElement.h>

#include <engine/utils/Vector2.h>

namespace engine {

class UI {
public:
	static ecs::Entity AddButton(ecs::Manager& manager, V2_int position, V2_int size, UIElement button_info) {
		auto button = manager.CreateEntity();
		button.AddComponent<UIComponent>(button_info);
		button.AddComponent<TransformComponent>(position);
		button.AddComponent<SizeComponent>(size);
		button.AddComponent<RenderComponent>();
		return button;
	}
private:

};

} // namespace engine