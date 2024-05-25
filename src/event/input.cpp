#include "protegon/input.h"

#include "core/game.h"

namespace ptgn {

namespace input {

void Update() {
	global::GetGame().input.Update();
}

void ForceUpdateMousePosition() {
	global::GetGame().input.ForceUpdateMousePosition();
}

V2_int GetMousePosition() {
	return global::GetGame().input.GetMousePosition();
}

int MouseScroll() {
	return global::GetGame().input.GetMouseScroll();
}

MouseState GetMouseState(Mouse button) {
	return global::GetGame().input.GetMouseState(button);
}

bool MousePressed(Mouse button) {
	return global::GetGame().input.MousePressed(button);
}

bool MouseReleased(Mouse button) {
	return global::GetGame().input.MouseReleased(button);
}

bool MouseDown(Mouse button) {
	return global::GetGame().input.MouseDown(button);
}

bool MouseUp(Mouse button) {
	return global::GetGame().input.MouseUp(button);
}

bool KeyPressed(Key key) {
	return global::GetGame().input.KeyPressed(key);
}

bool KeyReleased(Key key) {
	return global::GetGame().input.KeyReleased(key);
}

bool KeyDown(Key key) {
	return global::GetGame().input.KeyDown(key);
}

bool KeyUp(Key key) {
	return global::GetGame().input.KeyUp(key);
}

milliseconds GetMouseHeldTime(Mouse button) {
	return global::GetGame().input.GetMouseHeldTime(button);
}

} // namespace input

} // namespace ptgn