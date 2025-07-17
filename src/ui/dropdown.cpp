#include "ui/dropdown.h"

#include <vector>

#include "common/assert.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "rendering/api/origin.h"
#include "scene/scene.h"
#include "ui/button.h"

namespace ptgn {

Dropdown CreateDropdownButton(Scene& scene, bool start_open) {
	Button dropdown_button{ CreateButton(scene) };

	auto& i{ dropdown_button.Add<impl::DropdownInstance>() };
	i.start_open_ = start_open;
	dropdown_button.AddScript<impl::DropdownScript>();

	if (start_open) {
		Dropdown{ dropdown_button }.Open();
	} else {
		Dropdown{ dropdown_button }.Close();
	}

	return dropdown_button;
}

namespace impl {

void DropdownScript::OnButtonActivate() {
	Dropdown{ entity }.Toggle();
}

void DropdownItemScript::OnButtonActivate() {
	if (!entity.Has<impl::DropdownInstance>()) {
		PTGN_ASSERT(entity.HasParent());
		Dropdown{ entity.GetParent() }.Close();
	}
}

} // namespace impl

Dropdown::Dropdown(const Entity& entity) : Button{ entity } {}

void Dropdown::RecalculateButtonPositions() {
	PTGN_ASSERT(
		Has<impl::DropdownInstance>(), "Cannot recalculate button positions of invalid dropdown"
	);
	auto& i{ Get<impl::DropdownInstance>() };
	if (i.buttons_.empty()) {
		return;
	}
	V2_float size;
	V2_float parent_size{ GetSize() };
	// When button is unspecified, all dropdown buttons inherit size of parent.
	if (i.button_size_.IsZero()) {
		size = parent_size;
	} else {
		size = i.button_size_;
	}
	PTGN_ASSERT(!size.IsZero(), "Invalid size for dropdown button");
	V2_float parent_center{ GetOriginOffset(GetOrigin(), parent_size) };
	V2_float parent_edge{ parent_center - GetOriginOffset(i.origin_, parent_size) };
	V2_float next_to_parent{ parent_edge - GetOriginOffset(i.origin_, size) };
	V2_float button_offset{ GetOriginOffset(i.direction_, size) };
	V2_float offset{ next_to_parent };
	for (auto& button : i.buttons_) {
		button.SetPosition(offset);
		button.SetSize(size);
		button.SetOrigin(Origin::Center);
		offset -= 2.0f * button_offset;
	}
}

bool Dropdown::WillStartOpen() const {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set button size of invalid dropdown");
	if (HasParent()) {
		auto parent{ GetParent() };
		if (parent.Has<impl::DropdownInstance>()) {
			return Get<impl::DropdownInstance>().start_open_ && Dropdown{ parent }.WillStartOpen();
		}
	}
	return Get<impl::DropdownInstance>().start_open_;
}

void Dropdown::AddButton(Button button) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set button size of invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };

	button.SetParent(*this);

	if (WillStartOpen()) {
		button.Show();
		button.Enable();
	} else {
		button.Hide();
		button.Disable();
	}

	button.AddScript<impl::DropdownItemScript>();

	i.buttons_.emplace_back(button);

	RecalculateButtonPositions();
}

void Dropdown::SetButtonSize(const V2_float& button_size) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set button size of invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	if (i.button_size_ == button_size) {
		return;
	}
	i.button_size_ = button_size;
	RecalculateButtonPositions();
}

void Dropdown::SetButtonOffset(const V2_float& button_offset) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set button offset of invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	if (i.button_offset_ == button_offset) {
		return;
	}
	i.button_offset_ = button_offset;
	RecalculateButtonPositions();
}

void Dropdown::SetDropdownDirection(Origin dropdown_direction) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set dropdown direction of invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	if (i.direction_ == dropdown_direction) {
		return;
	}
	PTGN_ASSERT(
		dropdown_direction != Origin::Center, "Cannot set dropdown direction to be Origin::Center"
	);
	i.direction_ = dropdown_direction;
	RecalculateButtonPositions();
}

void Dropdown::SetDropdownOrigin(Origin dropdown_origin) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set dropdown origin of invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	if (i.origin_ == dropdown_origin) {
		return;
	}
	PTGN_ASSERT(i.origin_ != Origin::Center, "Cannot set dropdown origin to be Origin::Center");
	i.origin_ = dropdown_origin;
	RecalculateButtonPositions();
}

void Dropdown::Toggle() {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot toggle invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	i.open_ = !i.open_;
	if (i.open_) {
		Open();
	} else {
		Close(false);
	}
}

void Dropdown::Open() {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot open invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	i.open_ = true;
	for (auto& b : i.buttons_) {
		b.Enable();
		b.Show();
	}
	auto children{ GetChildren() };
	for (const auto& child : children) {
		if (child.Has<impl::DropdownInstance>()) {
			const auto& child_i{ child.Get<impl::DropdownInstance>() };
			if (child_i.start_open_) {
				Dropdown{ child }.Open();
			}
		}
	}
}

void Dropdown::Close(bool close_parents) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot close invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };
	i.open_ = false;
	for (auto& b : i.buttons_) {
		b.Disable();
		b.Hide();
	}
	if (close_parents && HasParent()) {
		auto parent{ GetParent() };
		if (parent.Has<impl::DropdownInstance>()) {
			Dropdown{ parent }.Close();
		}
	}
	auto children{ GetChildren() };
	for (const auto& child : children) {
		if (child.Has<impl::DropdownInstance>()) {
			Dropdown{ child }.Close(false);
		}
	}
}

} // namespace ptgn