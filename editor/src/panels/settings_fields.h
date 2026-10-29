#pragma once

#include <imgui.h>

#include <functional>
#include <string_view>
#include <utility>

#include "panels/inspector_fields.h"

namespace ptgn::editor::settings {

template <typename Get, typename Set, typename Draw>
bool EditSection(
	std::string_view label, Get&& get, Set&& set, Draw&& draw,
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
) {
	auto value{ std::invoke(std::forward<Get>(get)) };

	ImGui::PushID(label.data(), label.data() + label.size());

	auto title{ std::string{ label } };
	bool changed{ false };

	if (ImGui::CollapsingHeader(title.c_str(), flags)) {
		ImGui::Indent();
		changed = std::invoke(std::forward<Draw>(draw), value);
		ImGui::Unindent();
		ImGui::Spacing();
	}

	ImGui::PopID();

	if (changed) {
		std::invoke(std::forward<Set>(set), value);
	}

	return changed;
}

template <typename Get, typename Set>
bool EditSection(
	EditorContext& ctx, std::string_view label, Get&& get, Set&& set,
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
) {
	return EditSection(
		label, std::forward<Get>(get), std::forward<Set>(set),
		[&ctx](auto& value) { return inspector::DrawComponentContents(ctx, value); }, flags
	);
}

template <typename Get, typename Set>
bool EditValue(EditorContext& ctx, std::string_view label, Get&& get, Set&& set) {
	auto value{ std::invoke(std::forward<Get>(get)) };

	if (!inspector::DrawValue(ctx, label, value)) {
		return false;
	}

	std::invoke(std::forward<Set>(set), value);
	return true;
}

template <typename Get, typename Set>
bool EditValue(EditorContext& ctx, std::string_view label, Get&& get, Set&& set, inspector::FieldOptions options) {
	auto value{ std::invoke(std::forward<Get>(get)) };

	if (!inspector::DrawValue(ctx, label, value, options)) {
		return false;
	}

	std::invoke(std::forward<Set>(set), value);
	return true;
}

} // namespace ptgn::editor::settings