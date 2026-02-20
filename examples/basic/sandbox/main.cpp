#include <iostream>
#include <ostream>

#include "app/application.h"
#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "platform/input/events.h"
#include "platform/input/mouse.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/components/text_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

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

		EmitScene(ev); // scene-local inventory change

		EAnnounceGlobal g{ "picked up loot" };
		Emit(g); // global
	}

private:
	int* counter_ = nullptr;
};

class RestartButton : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<MouseDown>([this](auto& e) {
			if (e.button == Mouse::Left && !e.held) {
				EAnnounceGlobal g{ "Mouse down!" };
				Emit(g);
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

		app().renderer.SetGameSize(V2_int{ 320, 180 });

		PTGN_LOG("Working Directory: ", GetWorkingDirectory());
		auto a = app().assets.LoadAudio("test", "assets/music1.ogg");
		// auto f = app().assets.LoadFont("test", "assets/retro_gaming.ttf", 11);
		auto t = app().assets.LoadTexture("test", "assets/smile.png");
		// auto j = app().assets.LoadJson("test", "assets/dialogue.json");

		// app().audio.Play("test");

		auto sprite = CreateSprite(*this, t, {});

		PTGN_ASSERT(sprite.Has<Texture>());
		PTGN_ASSERT((sprite.Get<Texture>().GetSize() == V2_int{ 300, 300 }));

		auto sprite2 = CreateSprite(*this, "assets/smile.png", { 200, 0 });

		PTGN_ASSERT(sprite2.Has<Texture>());
		PTGN_ASSERT((sprite2.Get<Texture>().GetSize() == V2_int{ 300, 300 }));

		auto texture1 = sprite.Get<Texture>();
		auto texture2 = sprite2.Get<Texture>();

		auto text = CreateText(*this, "Hello World", color::Orange, 18.0f, {}, {});
		SetTextHD(text, false);

		PTGN_LOG("Loaded all assets!");

		Refresh();
	}

	void OnUpdate() override {
		/*PTGN_LOG("Master Volume: ", app().audio.GetVolume());
		PTGN_LOG("Volume: ", app().audio.GetVolume("test"));
		PTGN_LOG("Test audio is playing: ", app().audio.IsPlaying("test"));*/
		// PTGN_INFO("Updating test scene");
	}

	void OnExit() override {
		// PTGN_INFO("Exiting test scene");
	}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<KeyDown>([this](auto& e) {
			if (e.IsPressed(Key::Enter)) {
				app().audio.Stop("test");
				PTGN_LOG("Pressed enter");
				PTGN_LOG("Frame: ", app().GetFrameCount());
				app().renderer.SetGameSize(V2_int{ 800, 800 });
			} else if (e.IsPressed(Key::Space)) {
				app().audio.TogglePause("test");
				PTGN_LOG("Pressed space");
				PTGN_LOG("Frame: ", app().GetFrameCount());
				app().renderer.SetGameSize(V2_int{ 800, 600 });
			}
		});
		d.Dispatch<GameResized>([this](auto& e) {
			PTGN_LOG("Game resized: ", e.size);
			PTGN_LOG("Frame: ", app().GetFrameCount());
		});
		d.Dispatch<impl::DisplayResized>([this](auto& e) {
			PTGN_LOG("Display resized: ", e.size);
			PTGN_LOG("Frame: ", app().GetFrameCount());
		});
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{ { .window = { .resizeable = true } } };
	app.StartWith<AssetScene>("");

	return 0;
}