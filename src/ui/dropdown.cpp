#include "ui/dropdown.h"

#include <functional>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "core/script.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "ui/button.h"

namespace ptgn {

Dropdown CreateDropdownButton(Scene& scene, bool start_open) {
	Button dropdown_button{ CreateButton(scene) };

	auto& i{ dropdown_button.Add<impl::DropdownInstance>() };
	i.start_open_ = start_open;
	AddScript<impl::DropdownScript>(dropdown_button);

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
		PTGN_ASSERT(HasParent(entity));
		Dropdown{ GetParent(entity) }.Close();
	}
}

} // namespace impl

Dropdown::Dropdown(const Entity& entity) : Button{ entity } {}

Dropdown& Dropdown::SetSize(const V2_float& size) {
	Button::SetSize(size);
	if (HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownInstance>()) {
			Dropdown{ parent }.RecalculateButtonPositions();
		}
	}
	RecalculateButtonPositions();
	return *this;
}

Dropdown& Dropdown::SetOrigin(Origin origin) {
	SetDrawOrigin(*this, origin);
	RecalculateButtonPositions();
	return *this;
}

void Dropdown::RecalculateButtonPositions() {
	PTGN_ASSERT(
		Has<impl::DropdownInstance>(), "Cannot recalculate button positions of invalid dropdown"
	);

	auto& info{ Get<impl::DropdownInstance>() };

	if (info.buttons_.empty()) {
		return;
	}

	V2_float parent_size{ GetSize() };

	const auto get_size = [parent_size, &info](const auto& button) {
		if (auto size{ button.GetSize() }; !size.IsZero()) {
			return size;
		}
		if (!info.button_size_.IsZero()) {
			return info.button_size_;
		}
		return parent_size;
	};

	V2_float parent_center{ -GetOriginOffset(GetDrawOrigin(*this), parent_size) };
	V2_float parent_edge{ parent_center + GetOriginOffset(info.origin_, parent_size) };

	PTGN_ASSERT(info.buttons_.size() >= 1);
	const auto& first_button{ info.buttons_.front() };
	auto size{ get_size(first_button) };

	V2_float offset{ parent_edge + GetOriginOffset(info.origin_, size) };

	for (std::size_t i{ 0 }; i < info.buttons_.size(); ++i) {
		auto& button{ info.buttons_[i] };
		size = get_size(button);
		// First button offset goes in the direction of the dropdown origin, the rest go in the
		// direction of dropdown.
		if (i != 0) {
			offset += GetOriginOffset(info.direction_, size);
		}
		SetPosition(button, offset);
		button.SetSize(size);
		SetDrawOrigin(button, Origin::Center);
		// Offset is added separately while moving through dropdown buttons.
		offset += GetOriginOffset(info.direction_, size);
	}
}

bool Dropdown::WillStartOpen() const {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set button size of invalid dropdown");
	if (HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownInstance>()) {
			return Get<impl::DropdownInstance>().start_open_ && Dropdown{ parent }.WillStartOpen();
		}
	}
	return Get<impl::DropdownInstance>().start_open_;
}

void Dropdown::AddButton(Button button) {
	PTGN_ASSERT(Has<impl::DropdownInstance>(), "Cannot set button size of invalid dropdown");
	auto& i{ Get<impl::DropdownInstance>() };

	SetParent(button, *this);

	if (WillStartOpen()) {
		Show(button);
		button.Enable();
	} else {
		Hide(button);
		button.Disable();
	}

	AddScript<impl::DropdownItemScript>(button);

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
		Show(b);
	}
	if (!HasChildren(*this)) {
		return;
	}
	const auto& children{ GetChildren(*this) };
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
		Hide(b);
	}
	if (close_parents && HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownInstance>()) {
			Dropdown{ parent }.Close();
		}
	}
	if (!HasChildren(*this)) {
		return;
	}
	const auto& children{ GetChildren(*this) };
	for (const auto& child : children) {
		if (child.Has<impl::DropdownInstance>()) {
			Dropdown{ child }.Close(false);
		}
	}
}

} // namespace ptgn