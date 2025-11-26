#include "core/app/application.h"

#include <iostream>
#include <ostream>

#include "core/event/event.h"
#include "core/event/events.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/scripting/script.h"
#include "core/scripting/scripts.h"
#include "ecs/entity.h"
#include "ecs/manager.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct EInventoryChanged : Event<EInventoryChanged> {
	Entity who{};
	int delta	 = 0;
	int newCount = 0;
};

struct EAnnounceGlobal : Event<EAnnounceGlobal> {
	EAnnounceGlobal(const char* text) : text{ text } {}

	const char* text{};
};

struct EButtonClick : Event<EButtonClick> {
	Entity target{};
	int mouseButton = 0;
	int clicks		= 1;
};

class PlayerInventoryUI : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<EInventoryChanged>([this](auto& e) {
			if (e.who == entity) {
				std::cout << "[UI] inventory now " << e.newCount << " (delta " << e.delta << ")\n";
			}
			// no bool returned -> not "handled", keep bubbling
		});

		d.Dispatch<EAnnounceGlobal>([](auto& e) {
			std::cout << e.text << "\n";
			// also not handled, just reacts
		});
	}
};

class LootPickup : public Script {
public:
	explicit LootPickup(int* playerCount) : counter_(playerCount) {}

	void Collect(Entity player) {
		if (!counter_) {
			return;
		}
		*counter_ += 1;

		EInventoryChanged ev{};
		ev.who		= player;
		ev.delta	= +1;
		ev.newCount = *counter_;

		EmitScene(ev);							   // scene-local inventory change

		Emit(EAnnounceGlobal{ "picked up loot" }); // global
	}

private:
	int* counter_ = nullptr;
};

class RestartButton : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<MouseDown>([this](auto& e) {
			if (e.button == Mouse::Left && !e.held) {
				Emit(EAnnounceGlobal{ "Mouse down!" });
				return true; // handled -> stop bubbling
			}
			return false;
		});
	}
};

class SecondScene : public Scene {
public:
	void OnEnter() override {
		PTGN_INFO("Entered second scene");
	}

	void OnUpdate() override {
		// PTGN_INFO("Updating test scene");
	}

	void OnExit() override {
		// PTGN_INFO("Exiting test scene");
	}
};

class EventScene : public Scene {
public:
	void OnEnter() override {
		PTGN_INFO("Entered event scene");

		Entity player = CreateEntity();
		Entity loot	  = CreateEntity();

		Refresh();

		AddScript<PlayerInventoryUI>(player);
		AddScript<RestartButton>(player);
		int playerCount = 0;
		auto& pickup	= AddScript<LootPickup>(loot, &playerCount);

		// Simulate pickup
		pickup.Collect(player);
		pickup.Collect(player);

		// Simulate button click
		EButtonClick click{};
		click.target	  = player;
		click.mouseButton = 0;
		click.clicks	  = 1;

		events.Emit(click); // scene-local bubbling
	}
};

class AssetScene : public Scene {
public:
	void OnEnter() override {
		PTGN_INFO("Entered asset scene");

		// PTGN_LOG("GetExecutableDirectory: ", GetExecutableDirectory());
		auto m = app().assets.LoadMusic("resources/music1.ogg");
		auto s = app().assets.LoadSound("resources/sound2.ogg");
		auto f = app().assets.LoadFont("resources/retro_gaming.ttf", 11);
		auto t = app().assets.LoadTexture("resources/smile.png");
		auto j = app().assets.LoadJson("resources/dialogue.json");

		PTGN_LOG("Loaded all assets!");
	}

	void OnUpdate() override {
		// PTGN_INFO("Updating test scene");
	}

	void OnExit() override {
		// PTGN_INFO("Exiting test scene");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{};
	app.StartWith<AssetScene>("");

	return 0;
}