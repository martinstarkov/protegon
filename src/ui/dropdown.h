#pragma once

#include <vector>

#include "core/entity.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "ui/button.h"

namespace ptgn {

class Button;
class Manager;

namespace impl {

struct DropdownInstance {
	std::vector<Button> buttons_;

	// Whether dropdown is open or closed.
	bool start_open_{ false };
	bool open_{ false };

	// Default value of {} results in, each button having the size of the parent button.
	V2_float button_size_;
	// Fixed static offset for each of the dropdown buttons.
	V2_float button_offset_;
	// Which direction the dropdown drops relative to the parent button.
	Origin direction_{ Origin::CenterBottom };
	// Which side/edge the dropdown is on relative to the parent button.
	Origin origin_{ Origin::CenterBottom };
};

class DropdownScript : public Script<DropdownScript, ButtonScript> {
public:
	DropdownScript() = default;

	void OnButtonActivate() override;
};

class DropdownItemScript : public Script<DropdownItemScript, ButtonScript> {
public:
	DropdownItemScript() = default;

	void OnButtonActivate() override;
};

} // namespace impl

class Dropdown : public Button {
public:
	Dropdown() = default;
	Dropdown(const Entity& entity);

	Dropdown& SetSize(const V2_float& size);

	Dropdown& SetOrigin(Origin origin);

	void AddButton(Button button);

	// Set the size that each dropdown button will be.
	// If not specified, each button will have the size of the parent button.
	void SetButtonSize(const V2_float& button_size);

	// Specify a fixed static offset for each of the dropdown buttons.
	void SetButtonOffset(const V2_float& button_offset);

	// Set which direction the dropdown drops relative to the parent button.
	void SetDropdownDirection(Origin dropdown_direction);

	// Set the edge/corner on which the dropdown starts relative to the parent button.
	void SetDropdownOrigin(Origin dropdown_origin);

	void Toggle();
	void Open();
	void Close(bool close_parents = true);

private:
	[[nodiscard]] bool WillStartOpen() const;

	void RecalculateButtonPositions();
};

// @param open If true, dropdown starts in an open state.
Dropdown CreateDropdownButton(Manager& manager, bool start_open = false);

} // namespace ptgn