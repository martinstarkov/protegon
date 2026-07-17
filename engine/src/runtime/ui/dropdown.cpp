#include "runtime/ui/dropdown.h"

#include <concepts>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace impl {

void DropdownScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::ButtonPress>([this]() { Dropdown{ entity }.Toggle(); });
}

void DropdownItemScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::ButtonPress>([this]() {
		if (!HasParent(entity)) {
			PTGN_WARN("Cannot update dropdown item script if item has no parent");
			return;
		}

		Entity parent{ GetParent(entity) };

		if (!parent.Has<impl::DropdownData>()) {
			PTGN_WARN("Cannot update dropdown item script if item parent has no dropdown data");
			return;
		}

		Dropdown parent_dropdown{ parent };

		PushEvent<ptgn::event::DropdownItemPress>(
			parent_dropdown, parent_dropdown, Button{ entity }
		);

		// Nested dropdown roots are also dropdown items. Pressing them should open/close own
		// menu, not close the parent menu.
		if (entity.Has<impl::DropdownData>()) {
			return;
		}

		parent_dropdown.Close();
	});
}

} // namespace impl

void Dropdown::HideDropdownBranch(Button button) {
	if (button.Has<impl::DropdownData>()) {
		Dropdown{ button }.Close(false);
	}

	if (!button.Has<impl::DropdownItem>()) {
		PTGN_WARN("Cannot hide dropdown button which does not have dropdown item component");
		return;
	}

	auto& item{ button.Get<impl::DropdownItem>() };

	if (!item.enabled_state.has_value()) {
		item.enabled_state = impl::DropdownItem::EnabledState{
			.press = button.IsEnabled(false),
			.hover = button.IsEnabled(true),
		};
	}

	Hide(button);

	// Refresh after hiding so all button visual children are also hidden.
	button.SetEnabled(false, false, true);
}

void Dropdown::ShowDropdownItem(Button button) const {
	Show(button);

	if (!button.Has<impl::DropdownItem>()) {
		PTGN_WARN("Cannot show dropdown button which does not have dropdown item component");
		return;
	}

	auto& item{ button.Get<impl::DropdownItem>() };

	if (item.enabled_state.has_value()) {
		auto state{ item.enabled_state.value() };
		item.enabled_state.reset();

		button.SetEnabled(state.press, state.hover, true);
	} else {
		button.RefreshVisualState();
	}

	if (!button.Has<impl::DropdownData>()) {
		return;
	}

	Dropdown dropdown{ button };

	if (dropdown.Get<impl::DropdownData>().start_open) {
		dropdown.Open();
	} else {
		dropdown.Close(false);
	}
}

bool Dropdown::IsOpen() const {
	return Has<impl::DropdownData>() && Get<impl::DropdownData>().open;
}

bool Dropdown::WillStartOpen() const {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot check if dropdown with no dropdown data will start open");
		return false;
	}

	if (!Get<impl::DropdownData>().start_open) {
		return false;
	}

	if (!HasParent(*this)) {
		return true;
	}

	Entity parent{ GetParent(*this) };

	if (!parent.Has<impl::DropdownData>()) {
		return true;
	}

	return Dropdown{ parent }.IsOpen();
}

Dropdown& Dropdown::Size(V2_float size) {
	Button::Size(size);
	RecalculateButtonPositions();
	RecalculateParentDropdown(*this);
	return *this;
}

Dropdown& Dropdown::Size(float radius) {
	Button::Size(radius);
	RecalculateButtonPositions();
	RecalculateParentDropdown(*this);
	return *this;
}

Dropdown& Dropdown::Origin(ptgn::Origin origin) {
	Entity::Add<ptgn::Origin>(origin);
	RefreshVisualState();

	RecalculateButtonPositions();
	RecalculateParentDropdown(*this);

	return *this;
}

std::vector<Button> Dropdown::GetButtons() const {
	std::vector<Button> buttons;

	if (!HasChildren(*this)) {
		return buttons;
	}

	for (Entity child : GetChildren(*this)) {
		if (!child.Has<impl::DropdownItem>()) {
			continue;
		}

		buttons.emplace_back(child);
	}

	return buttons;
}

void Dropdown::RecalculateParentDropdown(Entity entity) const {
	if (!HasParent(entity)) {
		return;
	}

	Entity parent{ GetParent(entity) };

	if (!parent.Has<impl::DropdownData>()) {
		return;
	}

	Dropdown{ parent }.RecalculateButtonPositions();
}

void Dropdown::RecalculateButtonPositions() {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot recalculate button positions of dropdown with no dropdown data");
		return;
	}

	const auto& buttons{ GetButtons() };

	if (buttons.empty()) {
		return;
	}

	auto& info{ Get<impl::DropdownData>() };

	auto parent_shape{ GetSize() };

	auto transform{ GetWorldTransform(*this) };

	auto get_scaled_size = [transform](const std::variant<V2_float, float>& size) {
		return std::visit(
			[&]<typename T>(const T& s) {
				if constexpr (std::same_as<T, V2_float>) {
					return Rect{ s }.GetSize(transform);
				} else if constexpr (std::same_as<T, float>) {
					return Circle{ s }.GetSize(transform);
				} else {
					static_assert(false, "Incomplete visitor");
				}
			},
			size
		);
	};

	auto scaled_parent_size{ get_scaled_size(parent_shape) };

	const auto get_button_size = [parent_shape,
								  &info](const Button&) -> std::variant<V2_float, float> {
		if (info.button_size.has_value()) {
			return info.button_size.value();
		}

		return parent_shape;
	};

	V2_float parent_center{ GetOffset(GetOrDefault<ptgn::Origin>(), scaled_parent_size) };
	V2_float parent_edge{ parent_center - GetOffset(info.origin, scaled_parent_size) };

	const auto& first_button{ buttons.front() };
	auto shape_size{ get_button_size(first_button) };
	auto scaled_size{ get_scaled_size(shape_size) };

	V2_float offset{ parent_edge - GetOffset(info.origin, scaled_size) + info.button_offset };

	for (auto i{ 0uz }; i < buttons.size(); ++i) {
		Button button{ buttons[i] };
		shape_size	= get_button_size(button);
		scaled_size = get_scaled_size(shape_size);
		// First button offset goes in the direction of the dropdown origin, the rest go in the
		// direction of dropdown.
		if (i != 0) {
			offset -= GetOffset(info.direction, scaled_size);
		}
		SetPosition(button, offset);
		button.Add<ptgn::Origin>(ptgn::Origin::Center);
		std::visit([&](const auto& s) { button.Size(s); }, shape_size);
		// Offset is added separately while moving through dropdown buttons.
		offset -= GetOffset(info.direction, scaled_size);
	}
}

Dropdown& Dropdown::AddButton(Button button) {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot add button to dropdown with no dropdown data");
		return *this;
	}

	Entity old_parent;

	if (HasParent(button)) {
		old_parent = GetParent(button);
	}

	SetParent(button, *this);

	auto& item{ button.TryAdd<impl::DropdownItem>() };
	item.enabled_state.reset();

	if (!HasScript<impl::DropdownItemScript>(button)) {
		AddScript<impl::DropdownItemScript>(button);
	}

	if (old_parent && old_parent != *this && old_parent.Has<impl::DropdownData>()) {
		Dropdown{ old_parent }.RecalculateButtonPositions();
	}

	if (IsOpen()) {
		ShowDropdownItem(button);
	} else {
		HideDropdownBranch(button);
	}

	RecalculateButtonPositions();

	return *this;
}

Button Dropdown::AddItem(std::string_view text) {
	std::variant<V2_float, float> size;

	if (const auto& info{ Get<impl::DropdownData>() }; info.button_size.has_value()) {
		size = info.button_size.value();
	} else {
		size = GetSize();
	}

	return std::visit(
		[&](const auto& s) {
			Button button{ CreateButton(GetScene(), {}, s, ptgn::Origin::Center) };
			button.Add<Tag>("Dropdown Item");
			button.Text().Content(text);

			AddButton(button);
			return button;
		},
		size
	);
}

Dropdown& Dropdown::SetButtonSize(std::optional<V2_float> button_size) {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot set button size of dropdown with no dropdown data");
		return *this;
	}

	auto& info{ Get<impl::DropdownData>() };

	if (info.button_size == button_size) {
		return *this;
	}

	info.button_size = button_size;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::SetButtonOffset(V2_float button_offset) {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot set button offset of dropdown with no dropdown data");
		return *this;
	}

	auto& info{ Get<impl::DropdownData>() };

	if (info.button_offset == button_offset) {
		return *this;
	}

	info.button_offset = button_offset;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::SetDropdownDirection(ptgn::Origin dropdown_direction) {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot set dropdown direction of dropdown with no dropdown data");
		return *this;
	}

	if (dropdown_direction == ptgn::Origin::Center) {
		PTGN_WARN("Cannot set dropdown direction to Origin::Center");
		return *this;
	}

	auto& info{ Get<impl::DropdownData>() };

	if (info.direction == dropdown_direction) {
		return *this;
	}

	info.direction = dropdown_direction;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::SetDropdownOrigin(ptgn::Origin dropdown_origin) {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot set dropdown origin of dropdown with no dropdown data");
		return *this;
	}

	if (dropdown_origin == ptgn::Origin::Center) {
		PTGN_WARN("Cannot set dropdown origin to Origin::Center");
		return *this;
	}

	auto& info{ Get<impl::DropdownData>() };

	if (info.origin == dropdown_origin) {
		return *this;
	}

	info.origin = dropdown_origin;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::Toggle() {
	if (IsOpen()) {
		return Close(false);
	}

	return Open();
}

Dropdown& Dropdown::Open() {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot open dropdown with no dropdown data");
		return *this;
	}

	auto& info{ Get<impl::DropdownData>() };

	bool was_open{ info.open };
	info.open = true;

	for (const auto& button : GetButtons()) {
		ShowDropdownItem(button);
	}

	if (!was_open) {
		PushEvent<event::DropdownOpen>(*this, *this);
		PushEvent<event::DropdownToggle>(*this, *this, true);
	}

	return *this;
}

Dropdown& Dropdown::Close(bool close_parents) {
	if (!Has<impl::DropdownData>()) {
		PTGN_WARN("Cannot close dropdown with no dropdown data");
		return *this;
	}

	auto& info{ Get<impl::DropdownData>() };

	bool was_open{ info.open };
	info.open = false;

	for (const auto& button : GetButtons()) {
		HideDropdownBranch(button);
	}

	if (close_parents && HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownData>()) {
			Dropdown{ parent }.Close();
		}
	}

	if (was_open) {
		PushEvent<event::DropdownClose>(*this, *this);
		PushEvent<event::DropdownToggle>(*this, *this, false);
	}

	return *this;
}

Dropdown CreateDropdown(
	Scene& scene, Transform transform, V2_float size, Origin origin, bool start_open
) {
	Button button{ CreateButton(scene, transform, size, origin) };
	button.Add<Tag>("Dropdown Button");

	Dropdown dropdown{ button };

	auto& info{ dropdown.Add<impl::DropdownData>() };
	info.start_open = start_open;

	if (!HasScript<impl::DropdownScript>(dropdown)) {
		AddScript<impl::DropdownScript>(dropdown);
	}

	if (start_open) {
		dropdown.Open();
	} else {
		dropdown.Close(false);
	}

	return dropdown;
}

} // namespace ptgn