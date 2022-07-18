#pragma once

#include "math/Vector2.h"
#include "event/Inputs.h"

namespace ptgn {

namespace input {

// Updates the input handler key and mouse state information.
void Update();

/*
* @return The x and y positions of the mouse relative to the top left of the focused window.
*/
V2_int GetMouseScreenPosition();

/*
* @return The x and y positions of the mouse relative to the absolute coordinate system (ignoring camera offest).
*/
V2_int GetMouseAbsolutePosition();

/*
* @return True if mouse button is pressed, false otherwise.
*/
bool MousePressed(Mouse button);

/*
* @return True if mouse button is released, false otherwise.
*/
bool MouseReleased(Mouse button);

/*
* @return True the first cycle a mouse button is pressed, false otherwise.
*/
bool MouseDown(Mouse button);

/*
* @return True the first cycle a mouse button is released, false otherwise.
*/
bool MouseUp(Mouse button);

/*
* @return True if given key is pressed, false otherwise.
*/
bool KeyPressed(Key key);

/*
* @return True if given key is released, false otherwise.
*/
bool KeyReleased(Key key);

/*
* @return True the first cycle a key is pressed, false otherwise.
*/
bool KeyDown(Key key);

/*
* @return True the first cycle a key is released, false otherwise.
*/
bool KeyUp(Key key);

} // namespace input

} // namespace ptgn