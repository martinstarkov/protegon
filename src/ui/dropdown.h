#pragma once

#include <cstdint>
#include <vector>

#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/handle.h"

namespace ptgn {

class Button;
struct Rect;

namespace impl {

struct DropdownInstance {
	std::vector<Button> buttons_;

	// Dropdown is visible.
	bool visible_{ false };

	// Default value of {} results in,each button having the size of the parent button.
	V2_float button_size_;
	// Which direction the dropdown drops relative to the parent button.
	Origin direction_{ Origin::CenterBottom };
};

} // namespace impl

class Dropdown : public Handle<impl::DropdownInstance> {
public:
	// Note: Dropdown button layer info is overriden internally after the Dropdown class is added to
	// the parent button.
	Button& Add(const Button& button);

	// Set the size that each dropdown button will be.
	// If not specified, each button will have the size of the parent button.
	void SetButtonSize(const V2_float& button_size);

	// Set which direction the dropdown drops relative to the parent button.
	void SetDropdownDirection(Origin dropdown_direction);

	void Toggle();
	void Show();
	void Hide();

private:
	friend class Button;

	void Draw(const Rect& parent_button_rect, std::int32_t render_layer);
};

} // namespace ptgn