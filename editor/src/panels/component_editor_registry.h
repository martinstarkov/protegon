#pragma once

#include <imgui.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "panels/inspector_fields.h"
#include "core/math/transform.h"
#include "renderer/pipeline/render_state.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

inline constexpr std::string_view kTagComponentGroup{ "Tag Components" };

using ComponentEditorChangedCallback = void (*)(Entity entity);
/// @return Optional: If not nullopt, entry is read only. Value contains the reason for being read
/// only. If value is empty, reason is not displayed.
using ComponentEditorReadOnlyCallback	  = std::optional<std::string_view> (*)(Entity entity);
using ComponentEditorDrawContentsCallback = bool (*)(Entity entity);
using ComponentEditorDrawJsonCallback	  = bool (*)(json& value);
using ComponentEditorAddMenuCallback	  = void (*)(Entity entity, std::string_view label);

struct ComponentEditorOptions {
	std::string_view label{};
	std::optional<std::string_view> group;
	bool removable{ true };
	bool default_open{ false };
	bool addable{ true };
	bool draw_after_tags{ false };
	ComponentEditorChangedCallback on_changed{ nullptr };
	ComponentEditorReadOnlyCallback get_read_only_reason{ nullptr };
	ComponentEditorDrawContentsCallback draw_contents{ nullptr };
	ComponentEditorAddMenuCallback draw_add_menu{ nullptr };
};

struct ComponentEditorRegistration {
	std::optional<ComponentEditorOptions> options;
};

[[nodiscard]] constexpr ComponentEditorRegistration MakeComponentEditorRegistration() {
	return {};
}

[[nodiscard]] constexpr ComponentEditorRegistration MakeComponentEditorRegistration(
	ComponentEditorOptions options
) {
	return ComponentEditorRegistration{ .options = options };
}

struct ResolvedComponentEditorOptions {
	std::string label;
	std::string group;
	bool removable{ true };
	bool default_open{ false };
	bool addable{ true };
	bool draw_after_tags{ false };
	ComponentEditorChangedCallback on_changed{ nullptr };
	ComponentEditorReadOnlyCallback get_read_only_reason{ nullptr };
	ComponentEditorDrawContentsCallback draw_contents{ nullptr };
	ComponentEditorAddMenuCallback draw_add_menu{ nullptr };
};

struct RegisteredComponentEditor {
	std::size_t type_id{ 0 };
	std::string default_label;
	ComponentEditorOptions options;
	void (*draw)(
		Entity entity, const RegisteredComponent& component,
		const ResolvedComponentEditorOptions& options
	){ nullptr };
	ComponentEditorDrawJsonCallback draw_json{ nullptr };
};

namespace impl {

template <typename T>
void DrawRegisteredEditorComponent(
	Entity entity, const RegisteredComponent& component,
	const ResolvedComponentEditorOptions& options
);

template <typename T>
bool DrawRegisteredEditorComponentJson(json& input);

} // namespace impl

class ComponentEditorRegistry {
public:
	template <typename T>
	static bool Register(ComponentEditorRegistration registration = {}) {
		using Component = std::remove_cvref_t<T>;

		auto& entries{ MutableEntries() };
		auto type_id{ Hash<Component>() };

		ComponentEditorDrawJsonCallback draw_json{ nullptr };

		if constexpr (
			JsonSerializable<Component> && JsonDeserializable<Component> &&
			(std::default_initializable<Component> || ::ptgn::impl::JsonGettable<Component>)
		) {
			draw_json = &impl::DrawRegisteredEditorComponentJson<Component>;
		}

		for (auto& entry : entries) {
			if (entry.type_id != type_id) {
				continue;
			}

			// A later registration refreshes the typed adapter. Explicit options replace earlier
			// options, while an option-less registration preserves them.
			entry.default_label = inspector::TypeLabel<Component>();
			if (registration.options.has_value()) {
				entry.options = std::move(registration.options.value());
			}
			entry.draw = &impl::DrawRegisteredEditorComponent<Component>;
			entry.draw_json = draw_json;
			return false;
		}

		entries.push_back(
			RegisteredComponentEditor{
				.type_id	   = type_id,
				.default_label = inspector::TypeLabel<Component>(),
				.options	   = registration.options.value_or(ComponentEditorOptions{}),
				.draw		   = &impl::DrawRegisteredEditorComponent<Component>,
				.draw_json	   = draw_json,
			}
		);

		return true;
	}

	[[nodiscard]] static const RegisteredComponentEditor* Find(std::size_t type_id) {
		for (const auto& entry : Entries()) {
			if (entry.type_id == type_id) {
				return &entry;
			}
		}

		return nullptr;
	}

	[[nodiscard]] static ResolvedComponentEditorOptions Resolve(
		const RegisteredComponent& component, const RegisteredComponentEditor& editor
	) {
		ResolvedComponentEditorOptions resolved{
			.label = editor.default_label,
			.group = component.is_empty ? std::string{ kTagComponentGroup } : std::string{},
		};

		if (!editor.options.label.empty()) {
			resolved.label = editor.options.label;
		}

		if (editor.options.group.has_value()) {
			resolved.group = editor.options.group.value();
		}

		resolved.removable			  = editor.options.removable;
		resolved.default_open		  = editor.options.default_open;
		resolved.addable			  = editor.options.addable;
		resolved.draw_after_tags	  = editor.options.draw_after_tags;
		resolved.on_changed			  = editor.options.on_changed;
		resolved.get_read_only_reason = editor.options.get_read_only_reason;
		resolved.draw_contents		  = editor.options.draw_contents;
		resolved.draw_add_menu		  = editor.options.draw_add_menu;

		return resolved;
	}

	static bool DrawJson(const RegisteredComponent& component, json& value) {
		const auto* editor{ Find(component.type_id) };

		if (!editor || !editor->draw_json) {
			return false;
		}

		return editor->draw_json(value);
	}

	static bool DrawJson(std::string_view component_name, json& value) {
		const auto* component{ ComponentRegistry::Find(component_name) };
		return component && DrawJson(*component, value);
	}

	static void DrawComponents(Entity entity, bool draw_after_tags = false) {
		for (const auto& component : ComponentRegistry::Components()) {
			// Drawn manually.
			if (component.type_id == Hash<Transform>() || component.type_id == Hash<Depth>()) {
				continue;
			}

			const auto* editor{ Find(component.type_id) };

			if (!editor) {
				continue;
			}

			auto options{ Resolve(component, *editor) };

			if (component.is_empty || options.draw_after_tags != draw_after_tags) {
				continue;
			}

			editor->draw(entity, component, options);
		}
	}

	static void DrawTagComponents(Entity entity) {
		bool has_tags{ false };

		for (const auto& component : ComponentRegistry::Components()) {
			const auto* editor{ Find(component.type_id) };

			if (!editor || !component.is_empty || !component.has(entity)) {
				continue;
			}

			if (Resolve(component, *editor).group == kTagComponentGroup) {
				has_tags = true;
				break;
			}
		}

		if (!has_tags) {
			return;
		}

		if (!ImGui::CollapsingHeader(kTagComponentGroup.data())) {
			ImGui::Spacing();
			return;
		}

		ImGui::Indent();

		for (const auto& component : ComponentRegistry::Components()) {
			const auto* editor{ Find(component.type_id) };

			if (!editor || !component.is_empty || !component.has(entity)) {
				continue;
			}

			auto options{ Resolve(component, *editor) };

			if (options.group == kTagComponentGroup) {
				DrawTagComponent(entity, component, options);
			}
		}

		ImGui::Unindent();
	}

	static void DrawAddComponentMenu(Entity entity) {
		for (const auto& component : ComponentRegistry::Components()) {
			const auto* editor{ Find(component.type_id) };

			if (!editor) {
				continue;
			}

			auto options{ Resolve(component, *editor) };

			if (options.group.empty() && HasAddMenuEntry(entity, component, options)) {
				DrawAddComponentMenuItem(entity, component, options);
			}
		}

		std::vector<std::string> groups;

		for (const auto& component : ComponentRegistry::Components()) {
			const auto* editor{ Find(component.type_id) };

			if (!editor) {
				continue;
			}

			auto options{ Resolve(component, *editor) };

			if (options.group.empty() ||
				std::find(groups.begin(), groups.end(), options.group) != groups.end()) {
				continue;
			}

			groups.push_back(options.group);

			if (!GroupHasAddMenuEntries(entity, options.group) ||
				!ImGui::BeginMenu(options.group.c_str())) {
				continue;
			}

			for (const auto& candidate : ComponentRegistry::Components()) {
				const auto* candidate_editor{ Find(candidate.type_id) };

				if (!candidate_editor) {
					continue;
				}

				auto candidate_options{ Resolve(candidate, *candidate_editor) };

				if (candidate_options.group == options.group &&
					HasAddMenuEntry(entity, candidate, candidate_options)) {
					DrawAddComponentMenuItem(entity, candidate, candidate_options);
				}
			}

			ImGui::EndMenu();
		}
	}

private:
	[[nodiscard]] static const std::vector<RegisteredComponentEditor>& Entries() {
		return MutableEntries();
	}

	[[nodiscard]] static std::vector<RegisteredComponentEditor>& MutableEntries() {
		static std::vector<RegisteredComponentEditor> entries;
		return entries;
	}

	[[nodiscard]] static bool HasAddMenuEntry(
		Entity entity, const RegisteredComponent& component,
		const ResolvedComponentEditorOptions& options
	) {
		return options.addable && !component.has(entity);
	}

	[[nodiscard]] static bool GroupHasAddMenuEntries(Entity entity, std::string_view group) {
		for (const auto& component : ComponentRegistry::Components()) {
			const auto* editor{ Find(component.type_id) };

			if (!editor) {
				continue;
			}

			auto options{ Resolve(component, *editor) };

			if (options.group == group && HasAddMenuEntry(entity, component, options)) {
				return true;
			}
		}

		return false;
	}

	static void DrawTagComponent(
		Entity entity, const RegisteredComponent& component,
		const ResolvedComponentEditorOptions& options
	) {
		ImGui::Spacing();
		ImGui::PushID(static_cast<int>(component.type_id));
		ImGui::Selectable(options.label.c_str(), false);

		if (options.removable && ImGui::BeginPopupContextItem("TagComponentContextMenu")) {
			if (ImGui::MenuItem("Remove Component")) {
				component.remove(entity);
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
		ImGui::Spacing();
	}

	static void DrawAddComponentMenuItem(
		Entity entity, const RegisteredComponent& component,
		const ResolvedComponentEditorOptions& options
	) {
		if (options.draw_add_menu) {
			options.draw_add_menu(entity, options.label);
			return;
		}

		if (component.add_default) {
			if (ImGui::MenuItem(options.label.c_str())) {
				component.add_default(entity);
			}
			return;
		}

		ImGui::BeginDisabled();
		ImGui::MenuItem(options.label.c_str());
		ImGui::EndDisabled();
	}

	[[nodiscard]] static bool DrawComponentHeader(
		Entity entity, const RegisteredComponent& component,
		const ResolvedComponentEditorOptions& options
	) {
		ImGui::PushID(static_cast<int>(component.type_id));

		auto flags{ options.default_open ? ImGuiTreeNodeFlags_DefaultOpen
										 : ImGuiTreeNodeFlags_None };
		bool open{ ImGui::CollapsingHeader(options.label.c_str(), flags) };

		if (options.removable && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
			if (ImGui::MenuItem("Remove Component")) {
				component.remove(entity);
				open = false;
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
		return open && component.has(entity);
	}

	template <typename T>
	friend void impl::DrawRegisteredEditorComponent(
		Entity entity, const RegisteredComponent& component,
		const ResolvedComponentEditorOptions& options
	);
};

namespace impl {

template <typename T>
void DrawRegisteredEditorComponent(
	Entity entity, const RegisteredComponent& component,
	const ResolvedComponentEditorOptions& options
) {
	if constexpr (std::is_empty_v<T>) {
		return;
	} else {
		if (!component.has(entity)) {
			return;
		}

		if (!ComponentEditorRegistry::DrawComponentHeader(entity, component, options)) {
			ImGui::Spacing();
			return;
		}

		std::optional<std::string_view> read_only_reason;

		if (options.get_read_only_reason) {
			read_only_reason = options.get_read_only_reason(entity);
		}

		bool read_only{ read_only_reason.has_value() };

		if (read_only && !read_only_reason->empty()) {
			ImGui::TextDisabled(
				"%.*s", static_cast<int>(read_only_reason->size()), read_only_reason->data()
			);
		}

		ImGui::Indent();

		inspector::ReadOnlyScope read_only_scope{ read_only };

		bool changed{ options.draw_contents ? options.draw_contents(entity)
											: inspector::DrawComponentContents(entity.Get<T>()) };

		if (changed && !read_only && options.on_changed) {
			options.on_changed(entity);
		}

		ImGui::Unindent();
	}
}

template <typename T>
bool DrawRegisteredEditorComponentJson(json& input) {
	if constexpr (std::is_empty_v<T>) {
		return false;
	} else if constexpr (std::default_initializable<T>) {
		T value{};
		input.get_to(value);

		if (!inspector::DrawComponentContents(value)) {
			return false;
		}

		input = value;
		return true;
	} else {
		T value{ input.template get<T>() };

		if (!inspector::DrawComponentContents(value)) {
			return false;
		}

		input = value;
		return true;
	}
}

} // namespace impl

} // namespace ptgn::editor
