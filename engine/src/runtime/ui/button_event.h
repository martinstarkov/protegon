#pragma once

#include "runtime/ui/button.h"
#include "runtime/ui/dropdown.h"

namespace ptgn::event {

struct ButtonPress : public ptgn::impl::event::ButtonBasePress<Button> {
	using ButtonBasePress<Button>::ButtonBasePress;
};

struct ButtonHover : public ptgn::impl::event::ButtonBasePress<Button> {
	using ButtonBasePress<Button>::ButtonBasePress;
};

struct ButtonHoverStart : public ptgn::impl::event::ButtonBasePress<Button> {
	using ButtonBasePress<Button>::ButtonBasePress;
};

struct ButtonHoverStop : public ptgn::impl::event::ButtonBasePress<Button> {
	using ButtonBasePress<Button>::ButtonBasePress;
};

struct ToggleButtonToggle {
	ToggleButton button;
	bool toggled{ false };
};

struct ToggleButtonPress : public ptgn::impl::event::ButtonBasePress<ToggleButton> {
	using ButtonBasePress<ToggleButton>::ButtonBasePress;
};

struct ToggleButtonHover : public ptgn::impl::event::ButtonBasePress<ToggleButton> {
	using ButtonBasePress<ToggleButton>::ButtonBasePress;
};

struct ToggleButtonHoverStart : public ptgn::impl::event::ButtonBasePress<ToggleButton> {
	using ButtonBasePress<ToggleButton>::ButtonBasePress;
};

struct ToggleButtonHoverStop : public ptgn::impl::event::ButtonBasePress<ToggleButton> {
	using ButtonBasePress<ToggleButton>::ButtonBasePress;
};

struct DropdownPress : public ptgn::impl::event::ButtonBasePress<Dropdown> {
	using ButtonBasePress<Dropdown>::ButtonBasePress;
};

struct DropdownHover : public ptgn::impl::event::ButtonBasePress<Dropdown> {
	using ButtonBasePress<Dropdown>::ButtonBasePress;
};

struct DropdownHoverStart : public ptgn::impl::event::ButtonBasePress<Dropdown> {
	using ButtonBasePress<Dropdown>::ButtonBasePress;
};

struct DropdownHoverStop : public ptgn::impl::event::ButtonBasePress<Dropdown> {
	using ButtonBasePress<Dropdown>::ButtonBasePress;
};

} // namespace ptgn::event