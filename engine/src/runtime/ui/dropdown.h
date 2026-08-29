#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "serialization/serialize.h"

namespace ptgn {

class Button;
class Dropdown;
class Scene;

namespace event {

struct DropdownOpen;
struct DropdownClose;
struct DropdownToggle;
struct DropdownItemPress;

} // namespace event

namespace impl {

struct DropdownData {
	/// @brief Whether dropdown starts in an open state.
	bool start_open{ false };

	/// @brief Whether dropdown is currently open.
	bool open{ false };

	/// @brief Runtime only initialization flag used by DropdownSystem.
	bool initialized{ false };

	/// @brief Default value of {} means each item uses the parent dropdown button size.
	std::optional<V2_float> button_size{};

	/// @brief Fixed static offset applied to the dropdown item layout.
	V2_float button_offset{};

	/// @brief Direction in which dropdown items are stacked relative to the parent button.
	Origin direction{ Origin::CenterBottom };

	/// @brief Edge/corner on which the dropdown starts relative to the parent button.
	Origin origin{ Origin::CenterBottom };

	PTGN_REFLECT(DropdownData, start_open, open, button_size, button_offset, direction, origin)
	PTGN_REFLECT_READONLY(DropdownData, initialized)
};

struct DropdownEnabledState {
	bool press{ true };
	bool hover{ true };

	PTGN_REFLECT(DropdownEnabledState, press, hover)
};

/// @brief Marker for direct child buttons that are dropdown items.
struct DropdownItem {
	std::optional<DropdownEnabledState> enabled_state{};

	// Runtime layout anchor. The entity Transform remains the user-editable transform;
	// layout changes preserve the delta from this anchor.
	V2_float layout_position{};
	bool layout_initialized{ false };

	PTGN_REFLECT_VALUE(DropdownItem, enabled_state)
};

struct DropdownSystem {
	/// @brief Makes DropdownData and DropdownItem entities ordinary buttons automatically.
	static void Prepare(Scene& scene);

	/// @brief Applies dropdown root and item behavior when ButtonPress is emitted.
	static void OnEvent(Entity entity, Event event);

private:
	static void OnButtonPress(Entity entity);
};

} // namespace impl

class Dropdown : public Button {
public:
	using Button::Button;

	bool IsOpen() const;
	[[nodiscard]] bool WillStartOpen() const;

	Dropdown& Size(V2_float size);
	Dropdown& Size(float radius);

	Dropdown& Origin(Origin origin);

	/// @brief Recalculates dropdown item layout while preserving each item's user transform delta.
	Dropdown& RefreshLayout();

	/// @brief Adds an existing button as a direct child dropdown item.
	Dropdown& AddButton(Button button);

	/// @brief Creates a new item button, adds it as a dropdown item, and returns it.
	Button AddItem(std::string_view text);

	/// @brief Returns direct child buttons marked with impl::DropdownItem.
	std::vector<Button> GetButtons() const;

	/// @brief Set the size that each dropdown item button will be.
	/// If not specified, each item uses the parent dropdown button size.
	Dropdown& SetButtonSize(std::optional<V2_float> button_size);

	/// @brief Specify a fixed static offset for the dropdown item layout.
	Dropdown& SetButtonOffset(V2_float button_offset);

	/// @brief Set which direction the dropdown items stack relative to the parent button.
	Dropdown& SetDropdownDirection(ptgn::Origin dropdown_direction);

	/// @brief Set the edge/corner on which the dropdown starts relative to the parent button.
	Dropdown& SetDropdownOrigin(ptgn::Origin dropdown_origin);

	Dropdown& Toggle();
	Dropdown& Open();
	Dropdown& Close(bool close_parents = true);

	template <EventCallbackInvocable<event::DropdownOpen> F>
	Dropdown& OnOpen(F&& callback) {
		return OnEvent<event::DropdownOpen>(std::forward<F>(callback));
	}

	template <EventCallbackInvocable<event::DropdownClose> F>
	Dropdown& OnClose(F&& callback) {
		return OnEvent<event::DropdownClose>(std::forward<F>(callback));
	}

	template <EventCallbackInvocable<event::DropdownToggle> F>
	Dropdown& OnToggle(F&& callback) {
		return OnEvent<event::DropdownToggle>(std::forward<F>(callback));
	}

	template <EventCallbackInvocable<event::DropdownItemPress> F>
	Dropdown& OnItemPress(F&& callback) {
		return OnEvent<event::DropdownItemPress>(std::forward<F>(callback));
	}

private:
	friend struct impl::DropdownSystem;

	void HideDropdownBranch(Button button);
	void ShowDropdownItem(Button button) const;

	template <typename E, EventCallbackInvocable<E> F>
	Dropdown& OnEvent(F&& callback) {
		AddScript<impl::EventScript<E>>(
			*this, impl::MakeEventCallback<E>(std::forward<F>(callback))
		);
		return *this;
	}

	void RecalculateParentDropdown(Entity entity) const;
	void RecalculateButtonPositions();
};

namespace event {

struct DropdownOpen {
	Dropdown dropdown;
};

struct DropdownClose {
	Dropdown dropdown;
};

struct DropdownToggle {
	Dropdown dropdown;
	bool open{ false };
};

struct DropdownItemPress {
	Dropdown dropdown;
	Button item;
};

} // namespace event

/// @param start_open If true, dropdown starts in an open state.
Dropdown CreateDropdown(
	Scene& scene, Transform transform, V2_float size, Origin origin = Origin::Center,
	bool start_open = false
);

} // namespace ptgn
