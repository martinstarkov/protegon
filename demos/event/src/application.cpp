#include <functional>

#include "common/assert.h"
#include "core/game.h"
#include "core/window.h"
#include "debug/log.h"
#include "events/event.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class EventScene : public Scene {
public:
	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);

		auto event_observer = &game.event;

		PTGN_ASSERT(!game.event.window.IsSubscribed(event_observer));
		game.event.window.Subscribe(
			event_observer, std::function([&](WindowEvent type, const Event& e) {
				switch (type) {
					case WindowEvent::Resizing: {
						const auto& es{ static_cast<const WindowResizingEvent&>(e) };
						PTGN_LOG("Resizing window, new size: ", es.size);
						break;
					}
					case WindowEvent::Resized: {
						const auto& es{ static_cast<const WindowResizedEvent&>(e) };
						PTGN_LOG("Finished resizing window, final size: ", es.size);
						break;
					}
					case WindowEvent::Quit: {
						const auto& es{ static_cast<const WindowQuitEvent&>(e) };
						PTGN_LOG("Quit window event");
						break;
					}
					default: break;
				}
			})
		);
		game.event.mouse.Subscribe(
			event_observer, std::function([&](MouseEvent type, const Event& e) {
				switch (type) {
					case MouseEvent::Down: {
						const auto& es{ static_cast<const MouseDownEvent&>(e) };
						PTGN_LOG("Mouse down, mouse: ", static_cast<int>(es.mouse));
						break;
					}
					case MouseEvent::Up: {
						const auto& es{ static_cast<const MouseUpEvent&>(e) };
						PTGN_LOG("Mouse up, mouse: ", static_cast<int>(es.mouse));
						break;
					}
					case MouseEvent::Scroll: {
						const auto& es{ static_cast<const MouseScrollEvent&>(e) };
						PTGN_LOG("Mouse scroll, amount: ", es.scroll);
						break;
					}
					case MouseEvent::Move: {
						const auto& es{ static_cast<const MouseMoveEvent&>(e) };
						PTGN_LOG("Mouse move: ", game.input.GetMousePosition());
						break;
					}
					default: break;
				}
			})
		);
		game.event.key.Subscribe(
			event_observer, std::function([&](KeyEvent type, const Event& e) {
				switch (type) {
					case KeyEvent::Pressed: {
						const auto& es{ static_cast<const KeyPressedEvent&>(e) };
						PTGN_LOG("Key pressed, key: ", static_cast<int>(es.key));
						break;
					}
					case KeyEvent::Down: {
						const auto& es{ static_cast<const KeyDownEvent&>(e) };
						PTGN_LOG("Key down, key: ", static_cast<int>(es.key));
						break;
					}
					case KeyEvent::Up: {
						const auto& es{ static_cast<const KeyUpEvent&>(e) };
						PTGN_LOG("Key up, key: ", static_cast<int>(es.key));
						break;
					}
					default: break;
				}
			})
		);
	}

	void Exit() override {
		game.event.UnsubscribeAll(&game.event);
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("EventScene", window_size);
	game.scene.Enter<EventScene>("");
	return 0;
}