#pragma once
#include <engine/Include.h>

#include "GameStartEvent.h"
#include "components/Components.h"

struct TitleScreenEvent {
	static void Invoke(ecs::Manager& manager, ecs::Manager& ui_manager) {
		manager.Clear();
		ui_manager.Clear();

		V2_int play_size = { 200, 100 };
		V2_int play_pos = engine::Engine::ScreenSize() / 2 - play_size / 2;
		auto play_button = engine::UI::AddButton<GameStartEvent>(ui_manager, manager, play_pos, play_size, engine::BLACK);
		play_button.AddComponent<HoverColorComponent>(engine::GREY);
		play_button.AddComponent<ActiveColorComponent>(engine::GOLD);
		play_button.AddComponent<TextComponent>("Start", engine::WHITE, 30, "resources/fonts/oswald_regular.ttf");
		play_button.AddComponent<TitleScreenComponent>();
	}
};