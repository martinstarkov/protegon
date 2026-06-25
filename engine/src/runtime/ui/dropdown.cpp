#include "runtime/ui/dropdown.h"

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
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

[[nodiscard]] std::optional<V2_float> GetShapeSize(
	Entity entity, const std::variant<Rect, Circle>& shape
) {
	auto transform{ GetWorldTransform(entity) };

	return std::visit(
		[&](const auto& value) -> std::optional<V2_float> { return value.GetSize(transform); },
		shape
	);
}

void HideDropdownBranch(Button button) {
	if (button.Has<impl::DropdownData>()) {
		Dropdown dropdown{ button };

		auto& info{ dropdown.Get<impl::DropdownData>() };
		info.open = false;

		for (Button child_button : dropdown.GetButtons()) {
			HideDropdownBranch(child_button);
		}
	}

	button.Disable();
	Hide(button);

	for (Entity part : button.Parts()) {
		Hide(part);
	}
}

void ShowDropdownItem(Button button) {
	Show(button);
	button.Enable();
	button.RefreshVisualState();

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

} // namespace

namespace impl {

void DropdownScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::ButtonPress>([this]() { Dropdown{ entity }.Toggle(); });
}

void DropdownItemScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::ButtonPress>([this]() {
		PTGN_ASSERT(HasParent(entity));

		Entity parent{ GetParent(entity) };

		if (!parent.Has<impl::DropdownData>()) {
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

bool Dropdown::IsOpen() const {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot query open state of invalid dropdown");
	return Get<impl::DropdownData>().open;
}

bool Dropdown::WillStartOpen() const {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot query start-open state of invalid dropdown");

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

Dropdown& Dropdown::SetShape(const std::optional<std::variant<Rect, Circle>>& shape) {
	Button::SetShape(shape);

	RecalculateButtonPositions();
	RecalculateParentDropdown(*this);

	return *this;
}

Dropdown& Dropdown::SetShape(Rect rect) {
	return SetShape(std::variant<Rect, Circle>{ rect });
}

Dropdown& Dropdown::SetShape(Circle circle) {
	return SetShape(std::variant<Rect, Circle>{ circle });
}

Dropdown& Dropdown::SetOrigin(Origin origin) {
	SetDrawOrigin(*this, origin);

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
	PTGN_ASSERT(
		Has<impl::DropdownData>(), "Cannot recalculate button positions of invalid dropdown"
	);

	auto buttons{ GetButtons() };

	if (buttons.empty()) {
		return;
	}

	auto& info{ Get<impl::DropdownData>() };

	auto parent_shape{ GetShape() };

	auto parent_size{ parent_shape.has_value()
						  ? GetShapeSize(*this, parent_shape.value()).value_or(V2_float{})
						  : V2_float{} };

	auto get_button_shape = [parent_shape, &info](Button button) -> std::variant<Rect, Circle> {
		if (auto rect{ button.TryGet<Rect>() }) {
			return *rect;
		}

		if (auto circle{ button.TryGet<Circle>() }) {
			return *circle;
		}

		if (info.button_size.has_value()) {
			return Rect{ info.button_size.value() };
		}

		PTGN_ASSERT(
			parent_shape.has_value(), "Cannot rely on parent dropdown shape if it has no shape set"
		);

		return parent_shape.value();
	};

	V2_float parent_center{ GetOffset(GetDrawOrigin(*this), parent_size) };
	V2_float parent_edge{ parent_center - GetOffset(info.origin, parent_size) };

	auto shape{ get_button_shape(buttons.front()) };
	auto size{ GetShapeSize(buttons.front(), shape).value_or(V2_float{}) };

	V2_float offset{ parent_edge - GetOffset(info.origin, size) + info.button_offset };

	for (auto i{ 0uz }; i < buttons.size(); ++i) {
		Button button{ buttons[i] };

		shape = get_button_shape(button);
		size  = GetShapeSize(button, shape).value_or(V2_float{});

		if (i != 0) {
			offset -= GetOffset(info.direction, size);
		}

		SetPosition(button, offset);

		std::visit([&](const auto& value) { button.SetShape(value); }, shape);

		SetDrawOrigin(button, Origin::Center);

		offset -= GetOffset(info.direction, size);
	}
}

Dropdown& Dropdown::AddButton(Button button) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot add item to invalid dropdown");

	SetParent(button, *this);

	button.TryAdd<impl::DropdownItem>();

	if (!HasScript<impl::DropdownItemScript>(button)) {
		AddScript<impl::DropdownItemScript>(button);
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
	std::optional<std::variant<Rect, Circle>> shape;

	if (const auto& info{ Get<impl::DropdownData>() }; info.button_size.has_value()) {
		shape = Rect{ info.button_size.value() };
	} else {
		shape = GetShape();
	}

	Button button{ CreateButton(GetScene(), {}, shape, Origin::Center) };
	button.SetText(text);

	AddButton(button);

	return button;
}

Dropdown& Dropdown::SetButtonSize(std::optional<V2_float> button_size) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set button size of invalid dropdown");

	auto& info{ Get<impl::DropdownData>() };

	if (info.button_size == button_size) {
		return *this;
	}

	info.button_size = button_size;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::SetButtonOffset(V2_float button_offset) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set button offset of invalid dropdown");

	auto& info{ Get<impl::DropdownData>() };

	if (info.button_offset == button_offset) {
		return *this;
	}

	info.button_offset = button_offset;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::SetDropdownDirection(Origin dropdown_direction) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set dropdown direction of invalid dropdown");
	PTGN_ASSERT(
		dropdown_direction != Origin::Center, "Cannot set dropdown direction to Origin::Center"
	);

	auto& info{ Get<impl::DropdownData>() };

	if (info.direction == dropdown_direction) {
		return *this;
	}

	info.direction = dropdown_direction;

	RecalculateButtonPositions();

	return *this;
}

Dropdown& Dropdown::SetDropdownOrigin(Origin dropdown_origin) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set dropdown origin of invalid dropdown");
	PTGN_ASSERT(dropdown_origin != Origin::Center, "Cannot set dropdown origin to Origin::Center");

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
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot open invalid dropdown");

	auto& info{ Get<impl::DropdownData>() };

	bool was_open{ info.open };
	info.open = true;

	for (Button button : GetButtons()) {
		ShowDropdownItem(button);
	}

	if (!was_open) {
		PushEvent<event::DropdownOpen>(*this, *this);
		PushEvent<event::DropdownToggle>(*this, *this, true);
	}

	return *this;
}

Dropdown& Dropdown::Close(bool close_parents) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot close invalid dropdown");

	auto& info{ Get<impl::DropdownData>() };

	bool was_open{ info.open };
	info.open = false;

	for (Button button : GetButtons()) {
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
	Scene& scene, Transform transform, const std::optional<std::variant<Rect, Circle>>& shape,
	Origin draw_origin, bool start_open
) {
	Button button{ CreateButton(scene, transform, shape, draw_origin) };

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