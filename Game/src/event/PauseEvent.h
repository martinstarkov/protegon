#pragma once

#include <engine/Include.h>
#include "components/Components.h"

struct PauseScreenEvent {
	static void Invoke(ecs::Entity& invoker, ecs::Manager& manager, ecs::Manager& ui_manager) {
		auto& pause = invoker.GetComponent<PauseScreenComponent>();
		if (pause.open) {
			pause.open = false;
			ui_manager.DestroyEntitiesWith<PauseScreenComponent>();
		} else {
			pause.open = true;
			auto pause_text = new engine::UITextBox("Paused", 30, "resources/fonts/oswald_regular.ttf", engine::WHITE, engine::BLACK);
			V2_int sb1 = { 200, 100 };
			V2_int pb1 = engine::Engine::ScreenSize() / 2 - sb1 / 2;
			auto b1 = engine::UI::AddStatic(&ui_manager, pb1, sb1, pause_text);
			b1.AddComponent<PauseScreenComponent>();
		}
	}
};