#include <iostream>
#include <ostream>

#include "app/application.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

struct EInventoryChanged : public Event<EInventoryChanged> {
	Entity who{};
	int delta	 = 0;
	int newCount = 0;
};

struct EAnnounceGlobal : public Event<EAnnounceGlobal> {
	EAnnounceGlobal(const char* text) : text{ text } {}

	const char* text{};
};

struct EButtonPress : public Event<EButtonPress> {
	EButtonPress() = default;

	EButtonPress(Entity target, int mouseButton, int presses) :
		target{ target }, mouseButton{ mouseButton }, presses{ presses } {}

	Entity target{};
	int mouseButton = 0;
	int presses		= 1;
};

class PlayerInventoryUI : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<EInventoryChanged>([this](const auto& e) {
			if (e.who == entity) {
				std::cout << "[UI] inventory now " << e.newCount << " (delta " << e.delta << ")\n";
			}
			// no bool returned -> not "handled", keep bubbling
		});

		d.Dispatch<EAnnounceGlobal>([](const auto& e) {
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
		d.Dispatch<event::MousePressed>([this](const auto& e) {
			if (e == Mouse::Left) {
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

		// Simulate button press
		EButtonPress press{};
		press.target	  = player;
		press.mouseButton = 0;
		press.presses	  = 1;

		ctx().event.Push<EButtonPress>({}, player, 0, 1); // scene-local bubbling
	}
};

class AssetScene : public Scene {
public:
	void OnEnter() override {
		PTGN_INFO("Entered asset scene");

		V2_int game_size{ 320, 180 };
		ctx().renderer.SetGameSize(game_size);
		V2_int window_size{ 1280, 720 };
		ctx().window.SetSize(window_size);

		// PTGN_LOG("Working Directory: ", GetWorkingDirectory());
		auto a = ctx().asset.LoadAudio("test", "assets/music.ogg");
		// auto f = ctx().asset.LoadFont("test", "assets/ttf.ttf", 11);
		auto t = ctx().asset.LoadTexture("test", "assets/smile.png");
		// auto j = ctx().asset.LoadJson("test", "assets/dialogue.json");

		// ctx().audio.Play("test");

		auto sprite = CreateSprite(*this, t, {});

		PTGN_ASSERT(sprite.Has<Texture>());
		PTGN_ASSERT((sprite.Get<Texture>().GetSize() == V2_int{ 300, 300 }));

		auto sprite2 = CreateSprite(*this, "test", { 200, 0 });

		PTGN_ASSERT(sprite2.Has<Texture>());
		PTGN_ASSERT((sprite2.Get<Texture>().GetSize() == V2_int{ 300, 300 }));

		auto arial = ctx().asset.LoadFont("arial", "assets/Arial.ttf", 72.0f);

		auto text = CreateText(*this, {}, "Hello World", color::Orange, 72.0f, arial);
		text.SetHD(true);

		/*auto button = CreateTextButton(*this, "Press me", color::Black);
		button.SetSize({ 200, 200 }).OnPress([this]() { PTGN_LOG("Pressed button!"); });*/

		// PTGN_LOG("Loaded all assets!");

		Refresh();
	}

	void OnUpdate() override {
		/*PTGN_LOG("Master Volume: ", ctx().audio.GetVolume());
		PTGN_LOG("Volume: ", ctx().audio.GetVolume("test"));
		PTGN_LOG("Test audio is playing: ", ctx().audio.IsPlaying("test"));*/
		// PTGN_INFO("Updating test scene");
	}

	void OnExit() override {
		// PTGN_INFO("Exiting test scene");
	}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::KeyPressed>([this](const auto& e) {
			if (e == Key::Enter) {
				PTGN_LOG("Pressed enter");
			} else if (e == Key::Space) {
				PTGN_LOG("Pressed space");
			}
		});
	}
};

int main(int, char**) {
	Application app{ "AssetScene" };
	app.StartWith<AssetScene>();
}