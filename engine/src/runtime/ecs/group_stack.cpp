#include "runtime/ecs/group_stack.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/interaction/draggable.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

UUID Id(Entity entity) {
	return entity && entity.Has<UUID>() ? entity.Get<UUID>() : UUID{ 0 };
}

std::vector<UUID> ActiveHistory(GroupStack stack) {
	if (!stack || !stack.Has<GroupStackData>()) {
		return {};
	}

	const auto& data{ stack.Get<GroupStackData>() };
	if (data.cleared) {
		return {};
	}

	std::vector<UUID> history{ data.history };
	if (history.empty() && stack.GetGroup(data.initial)) {
		history.push_back(data.initial);
	}

	std::erase_if(history, [stack](UUID id) { return !stack.GetGroup(id); });
	if (history.empty() && stack.GetGroup(data.initial)) {
		history.push_back(data.initial);
	}

	if (history.empty()) {
		auto groups{ stack.GetGroups() };
		if (!groups.empty()) {
			history.push_back(Id(groups.front()));
		}
	}

	return history;
}

void ResetInteraction(GroupStack stack) {
	if (!stack.HasScene()) {
		return;
	}

	for (auto [entity, data] : stack.GetScene().EntitiesWith<impl::ButtonData>()) {
		if (GetGroupStack(entity) == stack) {
			data.state	= impl::InternalButtonState::IdleUp;
			data.dirty |= impl::ButtonDirty::All;
		}
	}

	for (auto [entity, drag] : stack.GetScene().EntitiesWith<impl::Draggable>()) {
		if (GetGroupStack(entity) == stack && !IsGroupStackInteractive(entity)) {
			drag.dragging = false;
			drag.start	  = {};
			drag.offset	  = {};
		}
	}
}

struct GroupLocation {
	GroupEntity group{};
	GroupStack stack{};
};

GroupLocation Locate(Entity entity) {
	GroupEntity group{};
	for (std::size_t i{ 0 }; entity && i < kMaxParentDepth; ++i) {
		if (entity.Has<Group>() && HasParent(entity) && GetParent(entity).Has<GroupStackData>()) {
			group = GroupEntity{ entity };
		}

		if (entity.Has<GroupStackData>()) {
			return { group, GroupStack{ entity } };
		}

		if (!HasParent(entity)) {
			break;
		}

		entity = GetParent(entity);
	}

	return {};
}

bool IsDescendantOrSelf(Entity entity, Entity parent) {
	for (std::size_t i{ 0 }; entity && i < kMaxParentDepth; ++i) {
		if (entity == parent) {
			return true;
		}

		if (!HasParent(entity)) {
			return false;
		}

		entity = GetParent(entity);
	}

	return false;
}

struct GroupState {
	bool visible{ true };
	bool interactive{ true };
	bool show_disabled_style{ false };
	int stack_position{ 0 };
};

GroupState ResolveState(Entity entity) {
	GroupState state{};
	GroupLocation location{ Locate(entity) };
	if (!location.group || !location.stack) {
		return state;
	}

	const auto& stack_data{ location.stack.Get<GroupStackData>() };
	UUID id{ Id(location.group) };

	if (location.stack.HasScene() && !location.stack.GetScene().IsRuntime()) {
		if (stack_data.preview_visible) {
			state.visible = std::ranges::find(*stack_data.preview_visible, id) !=
							stack_data.preview_visible->end();
		} else {
			state.visible = location.group == location.stack.GetInitialGroup();
		}

		state.interactive = state.visible;
		auto groups{ location.stack.GetGroups() };
		for (std::size_t i{}; i < groups.size(); ++i) {
			if (groups[i] == location.group) {
				state.stack_position = static_cast<int>(i);
				break;
			}
		}
		return state;
	}

	std::vector<UUID> history{ ActiveHistory(location.stack) };
	auto it{ std::ranges::find(history, id) };
	if (it == history.end()) {
		state.visible	  = false;
		state.interactive = false;
		return state;
	}

	state.stack_position = static_cast<int>(std::distance(history.begin(), it));
	if (it == history.end() - 1) {
		return state;
	}

	state.interactive = false;
	const auto& group_data{ location.group.Get<Group>() };
	OverlayMode mode{ group_data.overlay };
	for (Entity current{ entity }; current && current != location.group;) {
		if (auto* override{ current.TryGet<OverlayOverride>() }) {
			mode = override->overlay;
			break;
		}

		if (!HasParent(current)) {
			break;
		}

		current = GetParent(current);
	}

	state.visible			  = mode != OverlayMode::Hide;
	state.show_disabled_style = state.visible && group_data.show_disabled_style;
	return state;
}

} // namespace

void GroupEntity::SetOverlayMode(OverlayMode mode) {
	TryAdd<Group>().overlay = mode;
}

void GroupEntity::SetShowDisabledStyle(bool enabled) {
	TryAdd<Group>().show_disabled_style = enabled;
}

void GroupEntity::SetBlockBackgroundInput(bool enabled) {
	TryAdd<Group>().block_background_input = enabled;
}

GroupEntity GroupStack::GetGroup(UUID uuid) const {
	if (!*this || !Has<GroupStackData>() || static_cast<int>(uuid) == 0 || !HasChildren(*this)) {
		return {};
	}

	for (Entity child : GetChildren(*this)) {
		if (child && child.Has<Group>() && child.Has<UUID>() && child.Get<UUID>() == uuid) {
			return GroupEntity{ child };
		}
	}

	return {};
}

std::vector<GroupEntity> GroupStack::GetGroups() const {
	std::vector<GroupEntity> groups;
	if (!*this || !Has<GroupStackData>() || !HasChildren(*this)) {
		return groups;
	}

	for (Entity child : GetChildren(*this)) {
		if (child && child.Has<Group>()) {
			groups.emplace_back(child);
		}
	}

	return groups;
}

GroupEntity GroupStack::AddGroup(std::string_view name) {
	if (!*this) {
		return {};
	}

	GroupEntity group{ HasScene() ? CreateGroup(EntityCreationContext{ GetScene() }, name)
								  : CreateGroup(EntityCreationContext{ GetManager() }, name) };
	AddChild(*this, group);

	auto& data{ TryAdd<GroupStackData>() };
	if (!GetGroup(data.initial)) {
		data.initial = Id(group);
		data.cleared = false;
	}

	return group;
}

void GroupStack::SetInitialGroup(Entity group) {
	if (!GetGroup(Id(group))) {
		return;
	}

	auto& data{ Get<GroupStackData>() };
	data.initial = Id(group);
	data.history.clear();
	data.cleared = false;
	ResetInteraction(*this);
}

GroupEntity GroupStack::GetInitialGroup() const {
	if (!Has<GroupStackData>()) {
		return {};
	}

	if (GroupEntity initial{ GetGroup(Get<GroupStackData>().initial) }) {
		return initial;
	}

	auto groups{ GetGroups() };
	return groups.empty() ? GroupEntity{} : groups.front();
}

GroupEntity GroupStack::GetCurrentGroup() const {
	if (!Has<GroupStackData>() || Get<GroupStackData>().cleared) {
		return {};
	}

	auto history{ ActiveHistory(*this) };
	return history.empty() ? GroupEntity{} : GetGroup(history.back());
}

std::vector<UUID> GroupStack::GetHistory() const {
	return ActiveHistory(*this);
}

std::size_t GroupStack::GetDepth() const {
	return GetHistory().size();
}

bool GroupStack::CanPop() const {
	return GetDepth() > 1;
}

bool GroupStack::Push(UUID group) {
	if (!GetGroup(group)) {
		return false;
	}

	auto& data{ Get<GroupStackData>() };
	auto history{ ActiveHistory(*this) };
	if (!history.empty() && history.back() == group) {
		return true;
	}

	std::erase(history, group);
	history.push_back(group);
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool GroupStack::Pop() {
	auto history{ ActiveHistory(*this) };
	if (history.size() <= 1) {
		return false;
	}

	history.pop_back();
	auto& data{ Get<GroupStackData>() };
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool GroupStack::Replace(UUID group) {
	if (!GetGroup(group)) {
		return false;
	}

	auto history{ ActiveHistory(*this) };
	if (!history.empty()) {
		history.pop_back();
	}

	std::erase(history, group);
	history.push_back(group);
	auto& data{ Get<GroupStackData>() };
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool GroupStack::PopTo(UUID group) {
	auto history{ ActiveHistory(*this) };
	auto it{ std::ranges::find(history, group) };
	if (it == history.end()) {
		return false;
	}

	history.erase(it + 1, history.end());
	auto& data{ Get<GroupStackData>() };
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool GroupStack::Set(UUID group) {
	if (!GetGroup(group)) {
		return false;
	}

	auto& data{ Get<GroupStackData>() };
	data.history = { group };
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

void GroupStack::Reset() {
	if (Has<GroupStackData>()) {
		auto& data{ Get<GroupStackData>() };
		data.history.clear();
		data.cleared = false;
		ResetInteraction(*this);
	}
}

void GroupStack::Clear() {
	if (Has<GroupStackData>()) {
		auto& data{ Get<GroupStackData>() };
		data.history.clear();
		data.cleared = true;
		ResetInteraction(*this);
	}
}

bool GroupStack::IsGroupPreviewVisible(Entity group) const {
	if (!GetGroup(Id(group))) {
		return false;
	}

	const auto& data{ Get<GroupStackData>() };
	if (data.preview_visible) {
		return std::ranges::find(*data.preview_visible, Id(group)) != data.preview_visible->end();
	}

	return group == GetInitialGroup();
}

void GroupStack::SetGroupPreviewVisible(Entity group, bool visible) {
	if (!GetGroup(Id(group))) {
		return;
	}

	auto& data{ Get<GroupStackData>() };
	if (!data.preview_visible) {
		data.preview_visible = std::vector<UUID>{};
		if (GroupEntity initial{ GetInitialGroup() }) {
			data.preview_visible->push_back(Id(initial));
		}
	}

	auto& selected{ *data.preview_visible };
	std::erase(selected, Id(group));
	if (visible) {
		selected.push_back(Id(group));
	}

	ResetInteraction(*this);
}

void GroupStack::ShowOnlyGroupPreview(Entity group) {
	if (!GetGroup(Id(group))) {
		return;
	}

	Get<GroupStackData>().preview_visible = std::vector<UUID>{ Id(group) };
	ResetInteraction(*this);
}

void GroupStack::ResetGroupPreview() {
	if (!Has<GroupStackData>()) {
		return;
	}

	Get<GroupStackData>().preview_visible.reset();
	ResetInteraction(*this);
}

GroupStack CreateGroupStack(EntityCreationContext context) {
	Entity entity{ context.CreateEntity(Tag{ "Group Stack" }) };
	entity.Add<GroupStackData>();
	return GroupStack{ entity };
}

GroupEntity CreateGroup(EntityCreationContext context, std::string_view name) {
	Entity entity{ context.CreateEntity(Tag{ std::string{ name } }) };
	entity.Add<Group>();
	return GroupEntity{ entity };
}

GroupEntity GetGroup(Entity entity) {
	for (std::size_t i{ 0 }; entity && i < kMaxParentDepth; ++i) {
		if (entity.Has<Group>()) {
			return GroupEntity{ entity };
		}

		if (!HasParent(entity)) {
			break;
		}

		entity = GetParent(entity);
	}

	return {};
}

GroupStack GetGroupStack(Entity entity) {
	return Locate(entity).stack;
}

bool IsGroupStackVisible(Entity entity) {
	return ResolveState(entity).visible;
}

bool IsGroupStackActive(Entity entity) {
	return ResolveState(entity).interactive;
}

bool IsGroupStackInteractive(Entity entity) {
	if (!entity || !ResolveState(entity).interactive) {
		return false;
	}

	if (!entity.HasScene()) {
		return true;
	}

	Entity active_modal{};
	for (auto [candidate, group] : entity.GetScene().EntitiesWith<Group>()) {
		if (!group.block_background_input || !IsGroupStackVisible(candidate) || !IsUI(candidate)) {
			continue;
		}

		GroupStack stack{ GetGroupStack(candidate) };
		if (!stack || stack.GetCurrentGroup() != candidate ||
			(candidate.HasScene() && !candidate.GetScene().IsRuntime())) {
			continue;
		}

		if (!active_modal || GetDepth(candidate) > GetDepth(active_modal) ||
			(GetDepth(candidate) == GetDepth(active_modal) &&
			 active_modal.WasCreatedBefore(candidate))) {
			active_modal = candidate;
		}
	}

	return !active_modal || IsDescendantOrSelf(entity, active_modal);
}

bool UseGroupDisabledStyle(Entity entity) {
	return ResolveState(entity).show_disabled_style;
}

int GetGroupStackDepthOffset(Entity entity) {
	return ResolveState(entity).stack_position * 10000;
}

} // namespace ptgn
