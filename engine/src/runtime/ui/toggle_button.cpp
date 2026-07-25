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
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_event.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

std::optional<ToggleButton> FindToggleButtonByKey(ToggleButtonGroup group, std::string_view key) {
	if (!HasChildren(group)) {
		return std::nullopt;
	}

	auto hash{ Hash(key) };

	for (Entity child : GetChildren(group)) {
		auto item{ child.TryGet<impl::ToggleButtonGroupItem>() };
		if (!item) {
			continue;
		}

		if (Hash(item->key) == hash) {
			return ToggleButton{ child };
		}
	}

	return std::nullopt;
}

} // namespace

namespace impl {

void ToggleButtonSystem::Prepare(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<ToggleButtonData>()) {
		entity.TryAdd<ButtonData>();
	}

	for (auto [entity, _item] : scene.EntitiesWith<ToggleButtonGroupItem>()) {
		entity.TryAdd<ToggleButtonData>();
		entity.TryAdd<ButtonData>();
	}
}

void ToggleButtonSystem::OnEvent(Entity entity, Event event) {
	if (!entity || !entity.Has<ToggleButtonData>()) {
		return;
	}

	event.Dispatch<ptgn::event::ButtonPress>([entity]() { OnButtonPress(entity); });
}

void ToggleButtonSystem::OnButtonPress(Entity entity) {
	ToggleButton self{ entity };

	if (!self.IsEnabled(false)) {
		return;
	}

	if (!self.Has<ToggleButtonGroupItem>()) {
		self.Toggle();
		return;
	}

	PTGN_ASSERT(
		HasParent(self),
		"ToggleButtonGroupItem must be a direct child of a ToggleButtonGroup"
	);

	Entity parent{ GetParent(self) };
	PTGN_ASSERT(
		parent.Has<ToggleButtonGroupData>(),
		"ToggleButtonGroupItem parent must have ToggleButtonGroupData"
	);

	ToggleButtonGroup{ parent }.SetActiveKey(self.Get<ToggleButtonGroupItem>().key);
}

} // namespace impl

bool ToggleButton::IsToggled() const {
	auto data{ TryGet<impl::ToggleButtonData>() };
	return data && data->toggled;
}

ToggleButton& ToggleButton::SetToggled(bool toggled) {
	TryAdd<impl::ButtonData>();
	auto& button{ TryAdd<impl::ToggleButtonData>() };

	if (button.toggled == toggled) {
		return *this;
	}

	button.toggled = toggled;

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

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

	toggle_button.TryAdd<impl::ToggleButtonData>();
	toggle_button.TryAdd<impl::ButtonData>();

	if (toggle_button.Has<impl::ToggleButtonGroupItem>()) {
		toggle_button.Get<impl::ToggleButtonGroupItem>().key = button_key;
	} else {
		toggle_button.Add<impl::ToggleButtonGroupItem>(button_key);
	}

	SetParent(toggle_button, *this);

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

	button->Remove<impl::ToggleButtonGroupItem>();
	SetParent(button.value(), Entity{});
}

void ToggleButtonGroup::SetActive(std::string_view button_key) {
	SetActiveKey(button_key);
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

void ToggleButtonGroup::SetActiveKey(impl::ToggleButtonGroupKey key) {
	auto& data{ TryAdd<impl::ToggleButtonGroupData>() };

	const bool same_as_current{ data.active.has_value() && data.active.value() == key };

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

		const bool active{ data.active.has_value() && item.key == data.active.value() };
		button.SetToggled(active);
	}
}

namespace {

ToggleButton CreateToggleButton(
	Scene& scene, Transform transform, auto shape, Origin origin, bool toggled
) {
	ToggleButton button{ CreateButton(scene, transform, shape, origin) };

	button.Add<Tag>("Toggle Button");
	button.TryAdd<impl::ToggleButtonData>();
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
	group.Entity::Add<Tag>("Toggle Button Group");
	group.Entity::Add<impl::ToggleButtonGroupData>();
	return group;
}

} // namespace ptgn
