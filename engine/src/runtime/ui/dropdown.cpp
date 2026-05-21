#include "runtime/ui/dropdown.h"

#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_event.h"

namespace ptgn {

namespace impl {

void DropdownScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::DropdownPress>([this]() { Dropdown{ entity }.Toggle(); });
}

void DropdownItemScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::DropdownPress>([this]() {
		if (!entity.Has<impl::DropdownData>()) {
			PTGN_ASSERT(HasParent(entity));
			Dropdown{ GetParent(entity) }.Close();
		}
	});
}

} // namespace impl

Dropdown::operator Button() const {
	return Button{ *this };
}

Dropdown& Dropdown::SetShape(const std::optional<std::variant<Rect, Circle>>& shape) {
	ButtonBase<Dropdown>::SetShape(shape);
	if (HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownData>()) {
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
		Has<impl::DropdownData>(), "Cannot recalculate button positions of invalid dropdown"
	);

	auto& info{ Get<impl::DropdownData>() };

	if (info.buttons_.empty()) {
		return;
	}

	auto parent_shape{ GetShape() };

	auto transform{ GetWorldTransform(*this) };

	auto get_shape_size = [transform](const std::variant<Rect, Circle>& shape) {
		return std::visit(
			[&]<typename T>(const T& s) {
				if constexpr (std::is_same_v<T, Rect> || std::is_same_v<T, Circle>) {
					return s.GetSize(transform);
				} else {
					static_assert(false, "Unknown shape type for dropdown button");
				}
			},
			shape
		);
	};

	auto parent_size{ parent_shape.has_value() ? std::visit(get_shape_size, *parent_shape)
											   : V2_float{} };

	const auto get_shape = [parent_shape, &info](const auto& button) -> std::variant<Rect, Circle> {
		if (auto rect{ button.template TryGet<Rect>() }) {
			return *rect;
		}
		if (auto circle{ button.template TryGet<Circle>() }) {
			return *circle;
		}
		if (info.button_size_.has_value()) {
			return Rect{ *info.button_size_ };
		}
		PTGN_ASSERT(
			parent_shape.has_value(), "Cannot rely on parent dropdown shape if it has no shape set"
		);
		return *parent_shape;
	};

	V2_float parent_center{ -GetOriginOffset(GetDrawOrigin(*this), parent_size) };
	V2_float parent_edge{ parent_center + GetOriginOffset(info.origin_, parent_size) };

	PTGN_ASSERT(info.buttons_.size() >= 1);
	const auto& first_button{ info.buttons_.front() };
	auto shape{ get_shape(first_button) };
	auto size{ get_shape_size(shape) };

	V2_float offset{ parent_edge + GetOriginOffset(info.origin_, size) };

	for (auto i{ 0uz }; i < info.buttons_.size(); ++i) {
		auto& button{ info.buttons_[i] };
		shape = get_shape(button);
		size  = get_shape_size(shape);
		// First button offset goes in the direction of the dropdown origin, the rest go in the
		// direction of dropdown.
		if (i != 0) {
			offset += GetOriginOffset(info.direction_, size);
		}
		SetPosition(button, offset);
		std::visit([&](const auto& s) { button.SetShape(s); }, shape);
		SetDrawOrigin(button, Origin::Center);
		// Offset is added separately while moving through dropdown buttons.
		offset += GetOriginOffset(info.direction_, size);
	}
}

bool Dropdown::WillStartOpen() const {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set button size of invalid dropdown");
	if (HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownData>()) {
			return Get<impl::DropdownData>().start_open_ && Dropdown{ parent }.WillStartOpen();
		}
	}
	return Get<impl::DropdownData>().start_open_;
}

Dropdown& Dropdown::AddButton(Button button) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set button size of invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };

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

	return *this;
}

Dropdown& Dropdown::SetButtonSize(std::optional<V2_float> button_size) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set button size of invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	if (i.button_size_ == button_size) {
		return *this;
	}
	i.button_size_ = button_size;
	RecalculateButtonPositions();
	return *this;
}

Dropdown& Dropdown::SetButtonOffset(V2_float button_offset) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set button offset of invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	if (i.button_offset_ == button_offset) {
		return *this;
	}
	i.button_offset_ = button_offset;
	RecalculateButtonPositions();
	return *this;
}

Dropdown& Dropdown::SetDropdownDirection(Origin dropdown_direction) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set dropdown direction of invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	if (i.direction_ == dropdown_direction) {
		return *this;
	}
	PTGN_ASSERT(
		dropdown_direction != Origin::Center, "Cannot set dropdown direction to be Origin::Center"
	);
	i.direction_ = dropdown_direction;
	RecalculateButtonPositions();
	return *this;
}

Dropdown& Dropdown::SetDropdownOrigin(Origin dropdown_origin) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot set dropdown origin of invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	if (i.origin_ == dropdown_origin) {
		return *this;
	}
	PTGN_ASSERT(i.origin_ != Origin::Center, "Cannot set dropdown origin to be Origin::Center");
	i.origin_ = dropdown_origin;
	RecalculateButtonPositions();
	return *this;
}

Dropdown& Dropdown::Toggle() {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot toggle invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	i.open_ = !i.open_;
	if (i.open_) {
		Open();
	} else {
		Close(false);
	}
	return *this;
}

Dropdown& Dropdown::Open() {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot open invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	i.open_ = true;
	for (auto& b : i.buttons_) {
		b.Enable();
		Show(b);
	}
	if (!HasChildren(*this)) {
		return *this;
	}
	const auto& children{ GetChildren(*this) };
	for (const auto& child : children) {
		if (child.Has<impl::DropdownData>()) {
			const auto& child_i{ child.Get<impl::DropdownData>() };
			if (child_i.start_open_) {
				Dropdown{ child }.Open();
			}
		}
	}
	return *this;
}

Dropdown& Dropdown::Close(bool close_parents) {
	PTGN_ASSERT(Has<impl::DropdownData>(), "Cannot close invalid dropdown");
	auto& i{ Get<impl::DropdownData>() };
	i.open_ = false;
	for (auto& b : i.buttons_) {
		b.Disable();
		Hide(b);
	}
	if (close_parents && HasParent(*this)) {
		Entity parent{ GetParent(*this) };
		if (parent.Has<impl::DropdownData>()) {
			Dropdown{ parent }.Close();
		}
	}
	if (!HasChildren(*this)) {
		return *this;
	}
	const auto& children{ GetChildren(*this) };
	for (const auto& child : children) {
		if (child.Has<impl::DropdownData>()) {
			Dropdown{ child }.Close(false);
		}
	}
	return *this;
}

Dropdown CreateDropdown(
	Scene& scene, V2_float position, const std::optional<std::variant<Rect, Circle>>& shape,
	Origin draw_origin, bool start_open
) {
	Dropdown dropdown_button{ CreateButton(scene, position, shape, draw_origin) };

	auto& i{ dropdown_button.Add<impl::DropdownData>() };
	i.start_open_ = start_open;
	AddScript<impl::DropdownScript>(dropdown_button);

	if (start_open) {
		Dropdown{ dropdown_button }.Open();
	} else {
		Dropdown{ dropdown_button }.Close();
	}

	return dropdown_button;
}

} // namespace ptgn