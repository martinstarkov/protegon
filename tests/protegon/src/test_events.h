#pragma once

#include "protegon/game.h"
#include "protegon/text.h"
#include "utility/debug.h"

using namespace ptgn;

bool TestEvents() {
	PTGN_INFO("Starting event tests...");

	bool event_observer{ false };

	game.event.window.Subscribe(
		(void*)&event_observer, std::function([&](WindowEvent type, const Event& e) {
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
		(void*)&event_observer, std::function([&](MouseEvent type, const Event& e) {
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
		(void*)&event_observer, std::function([&](KeyEvent type, const Event& e) {
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

	game.window.SetSize({ 800, 800 });
	game.window.Show();
	game.window.SetResizeable(true);

	game.renderer.SetClearColor(color::Black);
	game.PushLoopFunction([&]() {
		if (game.input.KeyDown(Key::ESCAPE)) {
			game.PopLoopFunction();
		}
		/* listening for events */
	});

	// game.event.UnsubscribeAll((void*)&event_observer);
	// game.window.SetResizeable(false);

	PTGN_INFO("All event tests passed!");
	return true;
}