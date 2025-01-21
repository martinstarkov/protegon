#include "ui/dropdown.h"

#include <cstdint>
#include <vector>

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "ui/button.h"
#include "utility/debug.h"

namespace ptgn {

Button& Dropdown::Add(const Button& button) {
	Create();
	auto& b{ Get().buttons_.emplace_back(button) };
	b.Set<ButtonProperty::Visibility>(false);
	b.Disable();
	return b;
}

void Dropdown::SetButtonSize(const V2_float& button_size) {
	Create();
	Get().button_size_ = button_size;
}

void Dropdown::SetDropdownDirection(Origin dropdown_direction) {
	Create();
	PTGN_ASSERT(
		dropdown_direction != Origin::Center, "Cannot set dropdown direction to be Origin::Center"
	);
	Get().direction_ = dropdown_direction;
}

void Dropdown::Toggle() {
	PTGN_ASSERT(IsValid(), "Cannot toggle invalid dropdown");
	auto& i{ Get() };
	i.visible_ = !i.visible_;
	if (i.visible_) {
		Show();
	} else {
		Hide();
	}
}

void Dropdown::Show() {
	PTGN_ASSERT(IsValid(), "Cannot show invalid dropdown");
	auto& i{ Get() };
	i.visible_ = true;
	for (auto& b : i.buttons_) {
		b.Enable();
		b.Set<ButtonProperty::Visibility>(true);
	}
}

void Dropdown::Hide() {
	PTGN_ASSERT(IsValid(), "Cannot hide invalid dropdown");
	auto& i{ Get() };
	i.visible_ = false;
	for (auto& b : i.buttons_) {
		b.Disable();
		b.Set<ButtonProperty::Visibility>(false);
	}
}

void Dropdown::Draw(const Rect& parent_button_rect, std::int32_t render_layer) {
	PTGN_ASSERT(IsValid(), "Cannot draw invalid dropdown");
	auto& i{ Get() };
	if (!i.visible_) {
		return;
	}
	Rect r{ {}, i.button_size_, i.direction_ };
	// When button is unspecified, all dropdown buttons inherit size of parent.
	if (r.size.IsZero()) {
		r.size = parent_button_rect.size;
	}
	// Start from corner of button.
	V2_float offset{ GetOffsetFromCenter(parent_button_rect.size, r.origin) };
	for (auto& button : i.buttons_) {
		offset	   += 2.0f * GetOffsetFromCenter(r.size, r.origin);
		r.position	= parent_button_rect.Center() + offset;
		button.SetRect(r);
		button.Set<ButtonProperty::RenderLayer>(render_layer);
		button.SetInternalOnActivate([this]() { Hide(); });
		button.Draw();
	}
}

} // namespace ptgn