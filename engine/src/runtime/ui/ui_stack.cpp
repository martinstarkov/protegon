#include "runtime/ui/ui_stack.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/interaction/draggable.h"
#include "runtime/scene/scene.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

std::vector<std::string> ActiveHistory(UIStack stack) {
	if (!stack || !stack.Has<UIStackData>()) {
		return {};
	}
	const auto& data{ stack.Get<UIStackData>() };

	if (data.cleared) {
		return {};
	}

	std::vector<std::string> history{ data.history };
	if (history.empty() && !data.initial.empty()) {
		history.push_back(data.initial);
	}
	std::erase_if(history, [stack](const std::string& key) { return !stack.GetLayer(key); });
	if (history.empty() && !data.initial.empty() && stack.GetLayer(data.initial)) {
		history.push_back(data.initial);
	}
	if (history.empty()) {
		auto screens{ stack.GetLayers() };
		if (!screens.empty()) {
			history.emplace_back(screens.front().GetKey());
		}
	}
	return history;
}

void ResetInteraction(UIStack stack) {
	if (!stack.HasScene()) {
		return;
	}
	for (auto [entity, data] : stack.GetScene().EntitiesWith<impl::ButtonData>()) {
		if (GetUIStack(entity) == stack) {
			data.state	= impl::InternalButtonState::IdleUp;
			data.dirty |= impl::ButtonDirty::All;
		}
	}
	for (auto [entity, drag] : stack.GetScene().EntitiesWith<impl::Draggable>()) {
		if (GetUIStack(entity) == stack && !IsUIStackInteractive(entity)) {
			drag.dragging = false;
			drag.start	  = {};
			drag.offset	  = {};
		}
	}
}

// Screen ancestry must be checked before the containing stack. Descendants never need
// explicit membership components and continue to work after being reparented.
struct ScreenLocation {
	UILayer screen{};
	UIStack stack{};
};

ScreenLocation Locate(Entity entity) {
	UILayer screen{};
	for (std::size_t i{ 0 }; entity && i < kMaxParentDepth; ++i) {
		if (!screen && entity.Has<UILayerData>()) {
			screen = UILayer{ entity };
		}
		if (entity.Has<UIStackData>()) {
			return { screen, UIStack{ entity } };
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

struct ScreenState {
	bool visible{ true };
	bool interactive{ true };
	bool show_disabled_style{ false };
	int stack_position{ 0 };
};

ScreenState ResolveState(Entity entity) {
	ScreenState state{};
	ScreenLocation location{ Locate(entity) };
	if (!location.screen || !location.stack) {
		return state;
	}

	const auto& stack_data{ location.stack.Get<UIStackData>() };
	// Editor layer visibility is a separate, unordered selection. Covered layer
	// policies and runtime navigation have no effect on the editor preview.
	if (location.stack.HasScene() && !location.stack.GetScene().IsRuntime()) {
		std::string_view key{ location.screen.Get<UILayerData>().key };
		if (stack_data.preview_visible) {
			state.visible = std::ranges::find(*stack_data.preview_visible, key) !=
							stack_data.preview_visible->end();
		} else {
			state.visible = key == location.stack.GetInitialLayer();
		}
		state.interactive = state.visible;
		auto layers{ location.stack.GetLayers() };
		for (std::size_t i{}; i < layers.size(); ++i) {
			if (layers[i] == location.screen) {
				state.stack_position = static_cast<int>(i);
				break;
			}
		}
		return state;
	}
	std::vector<std::string> history{ ActiveHistory(location.stack) };

	const auto& screen_data{ location.screen.Get<UILayerData>() };
	auto it{ std::ranges::find(history, screen_data.key) };
	if (it == history.end()) {
		state.visible	  = false;
		state.interactive = false;
		return state;
	}

	state.stack_position = static_cast<int>(std::distance(history.begin(), it));
	bool covered{ it != history.end() - 1 };
	if (!covered) {
		return state;
	}

	state.interactive = false;
	OverlayMode mode{ screen_data.overlay };
	// The closest override component wins, otherwise the layer's mode applies.
	for (Entity current{ entity }; current && current != location.screen;) {
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
	state.show_disabled_style = state.visible && screen_data.show_disabled_style;
	return state;
}

} // namespace

void UILayer::SetOverlayMode(OverlayMode mode) {
	TryAdd<UILayerData>().overlay = mode;
}

void UILayer::SetShowDisabledStyle(bool enabled) {
	TryAdd<UILayerData>().show_disabled_style = enabled;
}

void UILayer::SetBlockBackgroundInput(bool enabled) {
	TryAdd<UILayerData>().block_background_input = enabled;
}

std::string_view UILayer::GetKey() const {
	return Has<UILayerData>() ? std::string_view{ Get<UILayerData>().key } : std::string_view{};
}

UILayer UIStack::GetLayer(std::string_view key) const {
	if (!*this || !Has<UIStackData>() || !HasChildren(*this)) {
		return {};
	}
	for (Entity child : GetChildren(*this)) {
		if (child && child.Has<UILayerData>() && child.Get<UILayerData>().key == key) {
			return UILayer{ child };
		}
	}
	return {};
}

std::vector<UILayer> UIStack::GetLayers() const {
	std::vector<UILayer> screens;
	if (!*this || !Has<UIStackData>() || !HasChildren(*this)) {
		return screens;
	}
	for (Entity child : GetChildren(*this)) {
		if (child && child.Has<UILayerData>()) {
			screens.emplace_back(child);
		}
	}
	return screens;
}

UILayer UIStack::AddLayer(std::string_view key) {
	if (!*this || key.empty()) {
		return {};
	}
	if (UILayer existing{ GetLayer(key) }) {
		return existing;
	}
	UILayer screen{ HasScene() ? CreateUILayer(EntityCreationContext{ GetScene() }, key)
							   : CreateUILayer(EntityCreationContext{ GetManager() }, key) };
	AddChild(*this, screen);
	if (auto& data{ TryAdd<UIStackData>() }; data.initial.empty()) {
		data.initial = key;
		data.cleared = false;
	}
	return screen;
}

void UIStack::SetInitialLayer(std::string_view key) {
	if (GetLayer(key)) {
		auto& data{ TryAdd<UIStackData>() };
		data.initial = key;
		data.history.clear();
		data.cleared = false;
		ResetInteraction(*this);
	}
}

std::string_view UIStack::GetInitialLayer() const {
	return Has<UIStackData>() ? std::string_view{ Get<UIStackData>().initial } : std::string_view{};
}

std::string_view UIStack::GetCurrentLayer() const {
	if (!Has<UIStackData>()) {
		return {};
	}
	const auto& data{ Get<UIStackData>() };

	if (data.cleared) {
		return {};
	}

	if (!data.history.empty() && GetLayer(data.history.back())) {
		return data.history.back();
	}
	if (GetLayer(data.initial)) {
		return data.initial;
	}
	if (HasChildren(*this)) {
		for (Entity child : GetChildren(*this)) {
			if (child && child.Has<UILayerData>()) {
				return child.Get<UILayerData>().key;
			}
		}
	}
	return {};
}

std::vector<std::string> UIStack::GetHistory() const {
	return ActiveHistory(*this);
}

std::size_t UIStack::GetDepth() const {
	return GetHistory().size();
}

bool UIStack::CanPop() const {
	return GetDepth() > 1;
}

bool UIStack::Push(std::string_view key) {
	if (!GetLayer(key)) {
		return false;
	}
	auto& data{ Get<UIStackData>() };
	auto history{ ActiveHistory(*this) };
	if (!history.empty() && history.back() == key) {
		return true;
	}
	// A screen can occur only once: reentering a screen moves it to the top.
	std::erase(history, std::string{ key });
	history.emplace_back(key);
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool UIStack::Pop() {
	auto history{ ActiveHistory(*this) };
	if (history.size() <= 1) {
		return false;
	}
	history.pop_back();
	auto& data{ Get<UIStackData>() };
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool UIStack::Replace(std::string_view key) {
	if (!GetLayer(key)) {
		return false;
	}
	auto history{ ActiveHistory(*this) };
	if (!history.empty()) {
		history.pop_back();
	}
	std::erase(history, std::string{ key });
	history.emplace_back(key);
	auto& data{ Get<UIStackData>() };
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool UIStack::PopTo(std::string_view key) {
	auto history{ ActiveHistory(*this) };
	auto it{ std::ranges::find(history, key) };
	if (it == history.end()) {
		return false;
	}
	history.erase(it + 1, history.end());
	auto& data{ Get<UIStackData>() };
	data.history = std::move(history);
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

bool UIStack::Set(std::string_view key) {
	if (!GetLayer(key)) {
		return false;
	}
	auto& data{ Get<UIStackData>() };
	data.history = { std::string{ key } };
	data.cleared = false;
	ResetInteraction(*this);
	return true;
}

void UIStack::Reset() {
	if (Has<UIStackData>()) {
		auto& data{ Get<UIStackData>() };
		data.history.clear();
		data.cleared = false;
		ResetInteraction(*this);
	}
}

void UIStack::Clear() {
	if (Has<UIStackData>()) {
		auto& data{ Get<UIStackData>() };
		data.history.clear();
		data.cleared = true;
		ResetInteraction(*this);
	}
}

bool UIStack::IsLayerPreviewVisible(std::string_view key) const {
	if (!GetLayer(key)) {
		return false;
	}
	const auto& data{ Get<UIStackData>() };
	if (data.preview_visible) {
		return std::ranges::find(*data.preview_visible, key) != data.preview_visible->end();
	}
	return key == GetInitialLayer();
}

void UIStack::SetLayerPreviewVisible(std::string_view key, bool visible) {
	if (!GetLayer(key)) {
		return;
	}
	auto& data{ Get<UIStackData>() };
	if (!data.preview_visible) {
		data.preview_visible = std::vector<std::string>{};
		if (GetLayer(data.initial)) {
			data.preview_visible->push_back(data.initial);
		}
	}
	auto& selected{ *data.preview_visible };
	std::erase(selected, std::string{ key });
	if (visible) {
		selected.emplace_back(key);
	}
	ResetInteraction(*this);
}

void UIStack::ResetLayerPreview() {
	if (!Has<UIStackData>()) {
		return;
	}
	Get<UIStackData>().preview_visible.reset();
	ResetInteraction(*this);
}

UIStack CreateUIStack(EntityCreationContext context) {
	Entity entity{ context.CreateEntity(Tag{ "UI Stack" }) };
	entity.Add<UIStackData>();
	return UIStack{ entity };
}

UILayer CreateUILayer(EntityCreationContext context, std::string_view key) {
	Entity entity{ context.CreateEntity(Tag{ std::string{ key } }) };
	entity.Add<UILayerData>(UILayerData{ .key = std::string{ key } });
	return UILayer{ entity };
}

UILayer GetUILayer(Entity entity) {
	return Locate(entity).screen;
}

UIStack GetUIStack(Entity entity) {
	return Locate(entity).stack;
}

bool IsUIStackVisible(Entity entity) {
	return ResolveState(entity).visible;
}

bool IsUIStackInteractive(Entity entity) {
	if (!entity || !ResolveState(entity).interactive) {
		return false;
	}
	if (!entity.HasScene()) {
		return true;
	}
	// The foremost active layer requesting background input blocking captures input.
	Entity active_modal{};
	for (auto [candidate, screen] : entity.GetScene().EntitiesWith<UILayerData>()) {
		if (!screen.block_background_input || !IsUIStackVisible(candidate)) {
			continue;
		}
		UIStack stack{ GetUIStack(candidate) };
		if (!stack || stack.GetCurrentLayer() != screen.key ||
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

bool UseUIDisabledStyle(Entity entity) {
	return ResolveState(entity).show_disabled_style;
}

int GetUIStackDepthOffset(Entity entity) {
	// Reserve a screen depth band while leaving local depths unchanged.
	return ResolveState(entity).stack_position * 10000;
}

} // namespace ptgn
