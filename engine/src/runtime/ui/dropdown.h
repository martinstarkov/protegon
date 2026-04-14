#pragma once

#include <optional>
#include <variant>
#include <vector>


#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"

namespace ptgn {

class Button;
class Scene;

namespace impl {

struct DropdownData {
	std::vector<Button> buttons_;

	/// @brief Whether dropdown is open or closed.
	bool start_open_{ false };
	bool open_{ false };

	/// @brief Default value of {} results in, each button having the size of the parent button.
	std::optional<V2_float> button_size_;
	/// @brief Fixed static offset for each of the dropdown buttons.
	V2_float button_offset_;
	/// @brief Which direction the dropdown drops relative to the parent button.
	Origin direction_{ Origin::CenterBottom };
	/// @brief Which side/edge the dropdown is on relative to the parent button.
	Origin origin_{ Origin::CenterBottom };
};

class DropdownScript : public Script {
public:
	DropdownScript() = default;

	void OnEvent(EventDispatcher d) override;
};

class DropdownItemScript : public Script {
public:
	DropdownItemScript() = default;

	void OnEvent(EventDispatcher d) override;
};

} // namespace impl

class Dropdown : public impl::ButtonBase<Dropdown> {
public:
	Dropdown() = default;
	using impl::ButtonBase<Dropdown>::ButtonBase;
	operator Button() const;

	Dropdown& SetShape(const std::optional<std::variant<Rect, Circle>>& shape = {});

	Dropdown& SetOrigin(Origin origin);

	Dropdown& AddButton(Button button);

	/// @brief Set the size that each dropdown button will be.
	/// If not specified, each button will have the size of the parent button.
	Dropdown& SetButtonSize(std::optional<V2_float> button_size);

	/// @brief Specify a fixed static offset for each of the dropdown buttons.
	Dropdown& SetButtonOffset(V2_float button_offset);

	/// @brief Set which direction the dropdown drops relative to the parent button.
	Dropdown& SetDropdownDirection(Origin dropdown_direction);

	/// @brief Set the edge/corner on which the dropdown starts relative to the parent button.
	Dropdown& SetDropdownOrigin(Origin dropdown_origin);

	Dropdown& Toggle();
	Dropdown& Open();
	Dropdown& Close(bool close_parents = true);

private:
	[[nodiscard]] bool WillStartOpen() const;

	void RecalculateButtonPositions();
};

/// @param open If true, dropdown starts in an open state.
Dropdown CreateDropdown(
	Scene& manager, V2_float position, const std::optional<std::variant<Rect, Circle>>& shape = {},
	Origin draw_origin = Origin::Center, bool start_open = false
);

} // namespace ptgn