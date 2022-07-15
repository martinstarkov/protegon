#pragma once

#include "event/InputHandler.h"

namespace ptgn {

namespace input {

void Update() {
    auto& input_handler{ services::GetInputHandler() };
    input_handler.Update();
}

V2_int GetMouseScreenPosition() {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.GetMouseScreenPosition();
}

V2_int GetMouseAbsolutePosition() {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.GetMouseAbsolutePosition();
}

bool MousePressed(Mouse button) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.MousePressed(button);
}

bool MouseReleased(Mouse button) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.MouseReleased(button);
}

bool MouseDown(Mouse button) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.MouseDown(button);
}

bool MouseUp(Mouse button) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.MouseUp(button);
}

bool KeyPressed(Key key) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.KeyPressed(key);
}

bool KeyReleased(Key key) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.KeyReleased(key);
}

bool KeyDown(Key key) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.KeyDown(key);
}

bool KeyUp(Key key) {
    auto& input_handler{ services::GetInputHandler() };
    return input_handler.KeyUp(key);
}

} // namespace input

} // namespace ptgn