#include "protegon/input.h"

#include "core/global.h"

namespace ptgn {

namespace input {

void Update() {
	global::GetGame().systems.input.Update();
}

V2_int GetMousePosition() {
	return global::GetGame().systems.input.GetMousePosition();
}

int MouseScroll() {
	return global::GetGame().systems.input.GetMouseScroll();
}

MouseState GetMouseState(Mouse button) {
	return global::GetGame().systems.input.GetMouseState(button);
}

bool MousePressed(Mouse button) {
	return global::GetGame().systems.input.MousePressed(button);
}

bool MouseReleased(Mouse button) {
	return global::GetGame().systems.input.MouseReleased(button);
}

bool MouseDown(Mouse button) {
	return global::GetGame().systems.input.MouseDown(button);
}

bool MouseUp(Mouse button) {
	return global::GetGame().systems.input.MouseUp(button);
}

bool KeyPressed(Key key) {
	return global::GetGame().systems.input.KeyPressed(key);
}

bool KeyReleased(Key key) {
	return global::GetGame().systems.input.KeyReleased(key);
}

bool KeyDown(Key key) {
	return global::GetGame().systems.input.KeyDown(key);
}

bool KeyUp(Key key) {
	return global::GetGame().systems.input.KeyUp(key);
}

milliseconds GetMouseHeldTime(Mouse button) {
	return global::GetGame().systems.input.GetMouseHeldTime(button);
}

} // namespace input

} // namespace ptgn