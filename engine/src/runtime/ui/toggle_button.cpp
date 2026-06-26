#include "runtime/ui/toggle_button.h"

#include <optional>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
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

	if (!self.IsEnabled(false)) {
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

	if (!self.IsEnabled(false)) {
		return;
	}

	PTGN_ASSERT(self.Has<ToggleButtonGroupItem>());

	toggle_button_group_.SetActiveKey(self.Get<ToggleButtonGroupItem>().key);
}

} // namespace impl

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

	if (auto buttons{ GetButtons() };
		Get<impl::ToggleButtonGroupData>().always_active && buttons.size() == 1) {
		SetActive(button_key);
	}

	return toggle_button;
}

void ToggleButtonGroup::Remove(std::string_view button_key) const {
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
		const auto& item{ button.Get<impl::ToggleButtonGroupItem>() };

		bool active{ data.active.has_value() && item.key == data.active.value() };
		button.SetToggled(active);
	}
}

namespace {

ToggleButton CreateToggleButton(
	Scene& scene, Transform transform, auto shape, Origin origin, bool toggled
) {
	ToggleButton button{ CreateButton(scene, transform, shape, origin) };

	button.Add<impl::ToggleButtonData>();

	PTGN_ASSERT(
		!HasScript<impl::ToggleButtonScript>(button), "Toggle button cannot be part of a group"
	);

	AddScript<impl::ToggleButtonScript>(button);

	button.SetToggled(toggled);

	return button;
}

} // namespace

ToggleButton CreateToggleButton(
	Scene& scene, Transform transform, V2_float size, Origin origin, bool toggled
) {
	return CreateToggleButton<V2_float>(scene, transform, size, origin, toggled);
}

ToggleButton CreateToggleButton(
	Scene& scene, Transform transform, float radius, Origin origin, bool toggled
) {
	return CreateToggleButton<float>(scene, transform, radius, origin, toggled);
}

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene) {
	ToggleButtonGroup group{ scene.CreateEntity() };
	group.Entity::Add<impl::ToggleButtonGroupData>();
	return group;
}

} // namespace ptgn