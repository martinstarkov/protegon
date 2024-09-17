#pragma once

#include <functional>

#include "common.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "protegon/text.h"
#include "utility/debug.h"

struct TestGameEvents : public Test {
	void Shutdown() override {
		game.window.SetResizeable(false);
	}

	void Init() override {
		game.window.SetTitle("'ESC' (++category), Event Tests");

		game.window.SetResizeable(true);

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
						PTGN_LOG("Quit window");
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
						PTGN_LOG(
							"Mouse down, mouse: ", static_cast<int>(es.mouse), ", pos: ", es.current
						);
						break;
					}
					case MouseEvent::Up: {
						const auto& es{ static_cast<const MouseUpEvent&>(e) };
						PTGN_LOG(
							"Mouse up, mouse: ", static_cast<int>(es.mouse), ", pos: ", es.current
						);
						break;
					}
					case MouseEvent::Scroll: {
						const auto& es{ static_cast<const MouseScrollEvent&>(e) };
						PTGN_LOG("Mouse scroll, amount: ", es.scroll);
						break;
					}
					case MouseEvent::Move: {
						const auto& es{ static_cast<const MouseMoveEvent&>(e) };
						PTGN_LOG("Mouse move, current: ", es.current, ", prev: ", es.previous);
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
};

void TestEvents() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestGameEvents());

	AddTests(tests);
}