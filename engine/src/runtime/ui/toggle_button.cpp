#include "runtime/ui/toggle_button.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>
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
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

std::optional<ToggleButton> FindToggleButtonByKey(ToggleButtonGroup group, std::string_view key) {
	if (!HasChildren(group)) {
		return std::nullopt;
	}

	impl::ToggleButtonGroupKey target_key{ key };

	for (Entity child : GetChildren(group)) {
		auto item{ child.TryGet<impl::ToggleButtonGroupItem>() };
		if (!item) {
			continue;
		}

		if (item->key == target_key) {
			return ToggleButton{ child };
		}
	}

	return std::nullopt;
}

} // namespace

namespace impl {

void ToggleButtonScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::ButtonPress>(&ToggleButtonScript::OnButtonPress, this);
}

void ToggleButtonScript::OnButtonPress() const {
	ToggleButton self{ entity };

	if (!self.AsButton().IsEnabled(false)) {
		return;
	}

	// Grouped toggle buttons are controlled by ToggleButtonGroupScript.
	// Otherwise the button toggles itself first, then the group toggles it again.
	if (self.Has<ToggleButtonGroupItem>()) {
		return;
	}

	self.Toggle();
}

ToggleButtonGroupScript::ToggleButtonGroupScript(ToggleButtonGroup group) :
	toggle_button_group_{ group } {}

void ToggleButtonGroupScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::ButtonPress>(&ToggleButtonGroupScript::OnButtonPress, this);
}

void ToggleButtonGroupScript::OnButtonPress() {
	ToggleButton self{ entity };

	if (!self.AsButton().IsEnabled(false)) {
		return;
	}

	PTGN_ASSERT(self.Has<ToggleButtonGroupItem>());

	toggle_button_group_.SetActiveKey(self.Get<ToggleButtonGroupItem>().key);
}

} // namespace impl

ToggleButton Button::AddToggle(bool toggled) {
	ToggleButton toggle{ *this };

	if (!Has<impl::ToggleButtonData>()) {
		Add<impl::ToggleButtonData>();
	}

	if (!HasScript<impl::ToggleButtonScript>(*this)) {
		AddScript<impl::ToggleButtonScript>(*this);
	}

	toggle.SetToggled(toggled);

	return toggle;
}

bool Button::HasToggle() const {
	return Has<impl::ToggleButtonData>();
}

ToggleButton Button::AsToggle() const {
	PTGN_ASSERT(HasToggle(), "Button does not have toggle capability");
	return ToggleButton{ *this };
}

ToggleButton::ToggleButton(Entity entity) : Entity{ entity } {}

ToggleButton::operator Button() const {
	return Button{ *this };
}

Button ToggleButton::AsButton() const {
	return Button{ *this };
}

bool ToggleButton::IsToggled() const {
	auto data{ TryGet<impl::ToggleButtonData>() };
	return data && data->toggled;
}

ToggleButton& ToggleButton::SetToggled(bool toggled) {
	auto& data{ TryAdd<impl::ToggleButtonData>() };

	if (data.toggled == toggled) {
		return *this;
	}

	data.toggled = toggled;

	Button{ *this }.RefreshVisualState();

	PushEvent<event::ToggleButtonToggle>(*this, *this, toggled);

	return *this;
}

ToggleButton& ToggleButton::Toggle() {
	return SetToggled(!IsToggled());
}

ToggleButtonGroup::ToggleButtonGroup(Entity entity) : Entity{ entity } {}

void ToggleButtonGroup::SetAlwaysOneActive(
	bool always_active, std::optional<std::string_view> button_key
) {
	auto& data{ TryAdd<impl::ToggleButtonGroupData>() };
	data.always_active = always_active;

	if (!always_active) {
		return;
	}

	if (button_key.has_value()) {
		SetActive(button_key.value());
		return;
	}

	auto buttons{ GetButtons() };
	if (!buttons.empty()) {
		SetActiveKey(buttons.front().Get<impl::ToggleButtonGroupItem>().key);
	}
}

ToggleButton ToggleButtonGroup::Add(std::string_view button_key, ToggleButton toggle_button) {
	TryAdd<impl::ToggleButtonGroupData>();

	RemoveScript<impl::ToggleButtonScript>(toggle_button);

	if (!toggle_button.Has<impl::ToggleButtonData>()) {
		toggle_button.Add<impl::ToggleButtonData>();
	}

	toggle_button.Add<impl::ToggleButtonGroupItem>(impl::ToggleButtonGroupKey{ button_key });

	SetParent(toggle_button, *this);

	AddToggleScript(toggle_button);

	auto buttons{ GetButtons() };
	if (Get<impl::ToggleButtonGroupData>().always_active && buttons.size() == 1) {
		SetActive(button_key);
	}

	return toggle_button;
}

ToggleButton ToggleButtonGroup::Add(std::string_view button_key, Button button) {
	return Add(button_key, button.AddToggle(false));
}

void ToggleButtonGroup::Remove(std::string_view button_key) {
	auto button{ FindToggleButtonByKey(*this, button_key) };

	if (!button.has_value()) {
		return;
	}

	if (!HasScript<impl::ToggleButtonScript>(button.value())) {
		AddScript<impl::ToggleButtonScript>(button.value());
	}

	RemoveScript<impl::ToggleButtonGroupScript>(button.value());
	button.value().Remove<impl::ToggleButtonGroupItem>();
	SetParent(button.value(), Entity{});
}

void ToggleButtonGroup::SetActive(std::string_view button_key) {
	SetActiveKey(impl::ToggleButtonGroupKey{ button_key });
}

std::optional<ToggleButton> ToggleButtonGroup::GetActive() const {
	auto data{ TryGet<impl::ToggleButtonGroupData>() };
	if (!data || !data->active.has_value()) {
		return std::nullopt;
	}

	if (!HasChildren(*this)) {
		return std::nullopt;
	}

	for (Entity child : GetChildren(*this)) {
		auto item{ child.TryGet<impl::ToggleButtonGroupItem>() };
		if (!item) {
			continue;
		}

		if (item->key == data->active.value()) {
			return ToggleButton{ child };
		}
	}

	return std::nullopt;
}

std::vector<ToggleButton> ToggleButtonGroup::GetButtons() const {
	std::vector<ToggleButton> result;

	if (!HasChildren(*this)) {
		return result;
	}

	for (Entity child : GetChildren(*this)) {
		if (child.Has<impl::ToggleButtonGroupItem>()) {
			result.emplace_back(child);
		}
	}

	return result;
}

void ToggleButtonGroup::AddToggleScript(ToggleButton toggle_button) const {
	if (!HasScript<impl::ToggleButtonGroupScript>(toggle_button)) {
		AddScript<impl::ToggleButtonGroupScript>(toggle_button, *this);
	}
}

void ToggleButtonGroup::SetActiveKey(impl::ToggleButtonGroupKey key) {
	auto& data{ TryAdd<impl::ToggleButtonGroupData>() };

	bool same_as_current{ data.active.has_value() && data.active.value() == key };

	if (same_as_current && data.always_active) {
		return;
	}

	if (same_as_current) {
		data.active.reset();
	} else {
		data.active.emplace(key);
	}

	for (ToggleButton button : GetButtons()) {
		auto& item{ button.Get<impl::ToggleButtonGroupItem>() };

		bool active{ data.active.has_value() && item.key == data.active.value() };
		button.SetToggled(active);
	}
}

ToggleButton CreateToggleButton(
	Scene& scene, V2_float position, const std::optional<std::variant<Rect, Circle>>& shape,
	Origin draw_origin, bool toggled
) {
	Button button{ CreateButton(scene, position, shape, draw_origin) };
	return button.AddToggle(toggled);
}

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene) {
	ToggleButtonGroup group{ scene.CreateEntity() };
	group.Entity::Add<impl::ToggleButtonGroupData>();
	return group;
}

} // namespace ptgn