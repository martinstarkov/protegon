#pragma once

#include <engine/Include.h>
#include "components/Components.h"

struct PauseScreenEvent {
	static void Invoke(ecs::Entity& invoker) {
		if (invoker.IsAlive()) {
			assert(invoker.HasComponent<EventComponent>());
			auto& event = invoker.GetComponent<EventComponent>();
			auto& pause = invoker.GetComponent<PauseScreenComponent>();
			if (pause.open) {
				pause.open = false;
				event.scene.ui_manager.DestroyEntitiesWith<PauseScreenComponent>();
			} else {
				pause.open = true;

				V2_int pause_size = { 200, 100 };
				V2_int pause_pos = engine::Engine::GetScreenSize() / 2 - pause_size / 2;
				auto pause_button = engine::UI::AddText(event.scene.ui_manager, pause_pos, pause_size, engine::BLACK);
				pause_button.AddComponent<TextComponent>("Paused", engine::WHITE, 30, "resources/fonts/oswald_regular.ttf");
				pause_button.AddComponent<PauseScreenComponent>();

			}
		}
	}
};