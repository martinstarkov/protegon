#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "commands/entity/entity_reference.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_selection.h"
#include "core/util/hash.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
#include "panels/inspector_component_drawers.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/slider.h"

namespace ptgn::impl {

class Scripts;
struct RenderTargetDesc;

} // namespace ptgn::impl

namespace ptgn::editor::inspector {

template <typename T>
using ComponentState = std::optional<T>;

struct FeatureTargetKey {
	const Scene* scene{ nullptr };
	std::optional<UUID> entity{};
	std::optional<PrefabKey> prefab{};
	SerializedEntityPath prefab_entity_path{};

	bool operator==(const FeatureTargetKey&) const = default;
};

template <typename T>
void AssignEntityComponent(Entity entity, ComponentState<T> state) {
	if constexpr (std::same_as<T, ::ptgn::impl::ParticleEmitterData>) {
		const auto* registration{ ComponentRegistry::Find<T>() };

		if (!state) {
			if (entity.Has<T>()) {
				entity.Remove<T>();
			}
			return;
		}

		if (!registration) {
			return;
		}

		if (!entity.Has<T>()) {
			registration->AddDefault(entity);
		}

		json serialized = *state;
		registration->Deserialize(serialized, entity);
		return;
	}

	if (!state) {
		if (entity.Has<T>()) {
			entity.Remove<T>();
		}
		return;
	}

	if constexpr (std::is_empty_v<T>) {
		entity.TryAdd<T>();
	} else if (entity.Has<T>()) {
		entity.Get<T>() = *state;
	} else {
		entity.Add<T>(*state);
	}
}

template <typename T>
ComponentState<T> CapturePrefabComponent(
	const SerializedEntity& serialized
) {
	const auto* registration{
		ComponentRegistry::Find<T>()
	};

	if (!registration) {
		return std::nullopt;
	}

	const std::string name{
		registration->name
	};

	if constexpr (std::is_empty_v<T>) {
		return std::ranges::contains(
			serialized.tags,
			name
		)
				   ? ComponentState<T>{ T{} }
				   : std::nullopt;
	} else {
		const auto it{
			serialized.components.find(name)
		};

		if (it == serialized.components.end()) {
			return std::nullopt;
		}

		if constexpr (
			JsonDeserializable<T> &&
			std::default_initializable<T>
		) {
			T value{};

			try {
				it->second.get_to(value);
				return ComponentState<T>{ std::move(value) };
			} catch (...) {
				return std::nullopt;
			}
		} else if constexpr (
			::ptgn::impl::JsonGettable<T>
		) {
			try {
				return it->second.template get<T>();
			} catch (...) {
				return std::nullopt;
			}
		} else {
			return std::nullopt;
		}
	}
}

template <typename T>
void AssignPrefabComponent(
	SerializedEntity& serialized,
	ComponentState<T> state
) {
	const auto* registration{
		ComponentRegistry::Find<T>()
	};

	if (!registration) {
		return;
	}

	const std::string name{
		registration->name
	};

	if (!state) {
		if constexpr (std::is_empty_v<T>) {
			std::erase(
				serialized.tags,
				name
			);
		} else {
			serialized.components.erase(name);
		}

		return;
	}

	if constexpr (std::is_empty_v<T>) {
		if (!std::ranges::contains(
				serialized.tags,
				name
			)) {
			serialized.tags.emplace_back(name);
		}
	} else if constexpr (JsonSerializable<T>) {
		json value = *state;

		serialized.components.insert_or_assign(
			name,
			std::move(value)
		);
	}
}

template <typename Callback>
void InvokeEntityChanged(Callback callback, Entity entity) {
	if constexpr (!std::same_as<Callback, std::nullptr_t>) {
		if (callback) {
			callback(entity);
		}
	}
}

template <typename T>
void AssignEntityInspectorComponent(Entity entity, ComponentState<T> state) {
	// SliderData::line is local to the slider or enabled Track Transform. Moving the slider
	// transform must therefore not translate the stored line coordinates.
	AssignEntityComponent<T>(entity, std::move(state));
}

struct EntityInspectorTarget {
	EditorContext& ctx;
	Entity entity{};

	template <typename T>
	[[nodiscard]] static constexpr bool Supports() {
		return true;
	}

	[[nodiscard]] const void* Id() const {
		return std::addressof(entity.Get<UUID>());
	}

	[[nodiscard]] FeatureTargetKey GetFeatureTargetKey() const {
		return FeatureTargetKey{
			.scene	= std::addressof(entity.GetScene()),
			.entity = entity.Get<UUID>(),
		};
	}

	template <typename T>
	[[nodiscard]] ComponentState<T> Capture() const {
		if (!entity.Has<T>()) {
			return std::nullopt;
		}

		if constexpr (std::is_empty_v<T>) {
			return T{};
		} else if constexpr (std::same_as<T, ::ptgn::impl::ParticleEmitterData>) {
			const auto* registration{ ComponentRegistry::Find<T>() };
			if (!registration) {
				return std::nullopt;
			}

			json serialized;
			if (!registration->Serialize(serialized, entity)) {
				return std::nullopt;
			}

			T value{};
			try {
				serialized.get_to(value);
			} catch (...) {
				return std::nullopt;
			}

			return ComponentState<T>{ std::move(value) };
		} else {
			return entity.Get<T>();
		}
	}

	template <typename T, typename Callback = std::nullptr_t>
	void SetLive(ComponentState<T> state, Callback callback = nullptr) {
		AssignEntityInspectorComponent<T>(entity, std::move(state));
		InvokeEntityChanged(callback, entity);
		::ptgn::impl::SliderSystem::SynchronizeEntity(entity);
	}

	template <typename T, typename Callback = std::nullptr_t>
	auto MakeApply(Callback callback = nullptr) const {
		Editor* editor{ std::addressof(ctx.editor) };
		const EntityReference reference{ MakeEntityReference(entity) };

		return [editor, reference, callback](ComponentState<T> state) mutable {
			Entity resolved{ reference.Resolve(*editor) };

			if (!resolved) {
				return;
			}

			AssignEntityInspectorComponent<T>(resolved, std::move(state));
			InvokeEntityChanged(callback, resolved);
			::ptgn::impl::SliderSystem::SynchronizeEntity(resolved);
		};
	}

	[[nodiscard]] std::string GetName() const {
		return std::string{ entity.Get<Tag>() };
	}

	[[nodiscard]] std::string GetUUIDText() const {
		const auto& uuid{ entity.Get<UUID>() };

		return [&]<typename T>(const T& value) {
			if constexpr (JsonSerializable<T>) {
				json serialized = value;

				if (serialized.is_string()) {
					return serialized.template get<std::string>();
				}

				return serialized.dump();
			} else if constexpr (requires { std::to_string(value.value); }) {
				return std::to_string(value.value);
			} else if constexpr (requires { std::to_string(value.value()); }) {
				return std::to_string(value.value());
			} else {
				return std::to_string(Hash(value));
			}
		}(uuid);
	}

	void SetName(std::string name) {
		entity.Add<Tag>(std::move(name));
	}

	auto MakeNameApply() const {
		Editor* editor{ std::addressof(ctx.editor) };
		const EntityReference reference{ MakeEntityReference(entity) };

		return [editor, reference](std::string name) {
			Entity resolved{ reference.Resolve(*editor) };

			if (resolved) {
				resolved.Add<Tag>(std::move(name));
			}
		};
	}
};

struct PrefabInspectorTarget {
	EditorContext& ctx;
	PrefabKey key{};
	SerializedEntityPath entity_path{};
	SerializedEntity& prefab;

	template <typename T>
	[[nodiscard]] static constexpr bool Supports() {
		return std::is_empty_v<T> ||
			   (
				   JsonSerializable<T> &&
				   JsonDeserializable<T> &&
				   (
					   std::default_initializable<T> ||
					   ::ptgn::impl::JsonGettable<T>
				   )
			   );
	}

	[[nodiscard]] const void* Id() const {
		return std::addressof(prefab);
	}

	[[nodiscard]] FeatureTargetKey GetFeatureTargetKey() const {
		return FeatureTargetKey{
			.prefab = key,
			.prefab_entity_path = entity_path,
		};
	}

	template <typename T>
	[[nodiscard]] ComponentState<T> Capture() const {
		return CapturePrefabComponent<T>(prefab);
	}

	template <typename T, typename Callback = std::nullptr_t>
	void SetLive(
		ComponentState<T> state,
		Callback = nullptr
	) {
		AssignPrefabComponent<T>(prefab, std::move(state));
	}

	template <typename T, typename Callback = std::nullptr_t>
	auto MakeApply(Callback = nullptr) const {
		EditorContext* context{ std::addressof(ctx) };
		PrefabKey prefab_key{ key };
		SerializedEntityPath path{ entity_path };

		return [
			context,
			prefab_key,
			path = std::move(path)
		](ComponentState<T> state) mutable {
			auto& assets{ context->editor.GetAssetManager() };

			if (!assets.Has(prefab_key)) {
				return;
			}

			auto prefab_asset{
				::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_key)
			};

			auto* serialized{
				ResolveSerializedEntity(
					prefab_asset.get().root,
					path
				)
			};

			if (!serialized) {
				return;
			}

			AssignPrefabComponent<T>(
				*serialized,
				std::move(state)
			);

			assets.SavePrefab(prefab_key);
			context->local.state.is_dirty = true;
		};
	}

	[[nodiscard]] std::string GetName() const {
		return prefab.tag;
	}

	void SetName(std::string name) {
		prefab.tag = std::move(name);
	}

	auto MakeNameApply() const {
		EditorContext* context{ std::addressof(ctx) };
		PrefabKey prefab_key{ key };
		SerializedEntityPath path{ entity_path };

		return [
			context,
			prefab_key,
			path = std::move(path)
		](std::string name) {
			auto& assets{ context->editor.GetAssetManager() };

			if (!assets.Has(prefab_key)) {
				return;
			}

			auto prefab_asset{
				::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_key)
			};

			auto* serialized{
				ResolveSerializedEntity(
					prefab_asset.get().root,
					path
				)
			};

			if (!serialized) {
				return;
			}

			serialized->tag = std::move(name);
			assets.SavePrefab(prefab_key);
			context->local.state.is_dirty = true;
		};
	}
};

template <typename Target, typename T, typename Callback>
void TrackComponentState(
	Target& target, std::string_view label, ComponentState<T> before, ComponentState<T> after,
	bool changed, Callback callback
) {
	if (!changed) {
		return;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };
	const ImGuiID key{ ImGui::GetID("##ComponentEdit") };

	auto apply{ target.template MakeApply<T>(callback) };

	TrackUndoableInteraction(
		target.ctx, key, std::string{ label }, true, [apply, before]() mutable { apply(before); },
		[apply, after]() mutable { apply(after); }
	);
}

template <typename Target, typename T>
void TrackComponentState(
	Target& target, std::string_view label, ComponentState<T> before, ComponentState<T> after,
	bool changed
) {
	TrackComponentState(target, label, std::move(before), std::move(after), changed, nullptr);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredComponent(
	Target& target, std::string_view label, bool tree, Draw&& draw, Callback callback = nullptr
) {
	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool changed{ false };

	if (!before) {
		target.template SetLive<T>(T{}, callback);
		changed = true;
	}

	T value{ target.template Capture<T>().value_or(T{}) };

	if (tree) {
		const std::string node_label{ std::string{ label } + "##Tree" };

		if (ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
			ScopedIndent indent;
			changed |= std::invoke(std::forward<Draw>(draw), value);
			ImGui::TreePop();
		}
	} else {
		changed |= std::invoke(std::forward<Draw>(draw), value);
	}

	if (changed) {
		target.template SetLive<T>(std::move(value), callback);
	}

	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, std::string{ "Edit " } + std::string{ label }, std::move(before), std::move(after),
		changed, callback
	);

	return changed;
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawOptionalComponent(
	Target& target, std::string_view label, bool tree, Draw&& draw, bool contents_read_only = false,
	bool toggle_read_only = false, Callback callback = nullptr, T enabled_default = T{}
) {
	if (!Target::template Supports<T>()) {
		return false;
	}

	if (
		(contents_read_only || toggle_read_only) &&
		!target.ctx.local.settings.show_read_only_inspector_data
	) {
		return false;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool enabled{ before.has_value() };
	bool changed{ false };

	{
		ScopedDisabled disabled{ toggle_read_only };

		if (ImGui::Checkbox("##Enabled", &enabled)) {
			if (enabled) {
				target.template SetLive<T>(enabled_default, callback);
			} else {
				target.template SetLive<T>(std::nullopt, callback);
			}

			changed = true;
		}
	}

	ImGui::SameLine();

	T value{ target.template Capture<T>().value_or(T{}) };
	const float checkbox_offset{
		ImGui::GetFrameHeight() +
		ImGui::GetStyle().ItemSpacing.x
	};

	if constexpr (std::is_empty_v<T>) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label.data(), label.data() + label.size());
	} else if (tree) {
		const std::string node_label{ std::string{ label } + "##Tree" };

		const bool open{ ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth) };

		if (open) {
			ScopedIndent indent;
			ScopedPropertyLabelOffset label_offset{
				ImGui::GetStyle().IndentSpacing
			};
			ScopedDisabled disabled{ !enabled || contents_read_only };

			const bool contents_changed{ std::invoke(std::forward<Draw>(draw), value) };

			if (enabled && !contents_read_only && contents_changed) {
				target.template SetLive<T>(std::move(value), callback);
				changed = true;
			}

			ImGui::TreePop();
		}
	} else {
		ScopedPropertyLabelOffset label_offset{ checkbox_offset };
		ScopedDisabled disabled{ !enabled || contents_read_only };

		ImGui::BeginGroup();
		const bool contents_changed{ std::invoke(std::forward<Draw>(draw), value) };
		ImGui::EndGroup();

		if (enabled && !contents_read_only && contents_changed) {
			target.template SetLive<T>(std::move(value), callback);
			changed = true;
		}
	}

	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, std::string{ enabled ? "Edit " : "Disable " } + std::string{ label },
		std::move(before), std::move(after), changed, callback
	);

	return changed;
}

template <typename Target, typename T, typename Draw>
bool DrawReadOnlyExistingComponent(
	Target& target,
	std::string_view label,
	bool tree,
	Draw&& draw
) {
	if (
		!Target::template Supports<T>() ||
		!target.ctx.local.settings.show_read_only_inspector_data
	) {
		return false;
	}

	auto state{ target.template Capture<T>() };

	if (!state) {
		return false;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	if (tree) {
		const std::string node_label{ std::string{ label } + "##ReadOnlyTree" };

		if (ImGui::TreeNodeEx(
				node_label.c_str(),
				ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			ScopedIndent indent;
			ScopedPropertyLabelOffset label_offset{
				ImGui::GetStyle().IndentSpacing
			};
			ScopedDisabled disabled{ true };
			std::invoke(
				std::forward<Draw>(draw),
				*state
			);
			ImGui::TreePop();
		}
	} else {
		ScopedDisabled disabled{ true };
		std::invoke(
			std::forward<Draw>(draw),
			*state
		);
	}

	return false;
}

template <typename Target, typename T>
bool DrawReadOnlyExistingReflected(
	Target& target,
	std::string_view label,
	bool tree = true
) {
	return DrawReadOnlyExistingComponent<Target, T>(
		target,
		label,
		tree,
		[&target](T& value) {
			return DrawRegisteredComponentContents(
				target.ctx,
				Hash<T>(),
				std::addressof(value)
			);
		}
	);
}

template <typename Target, typename T>
bool DrawOptionalReflected(
	Target& target, std::string_view label, bool tree = true, bool contents_read_only = false,
	bool toggle_read_only = false
) {
	if constexpr (std::is_empty_v<T>) {
		return DrawOptionalComponent<Target, T>(
			target, label, false, [](T&) { return false; }, contents_read_only, toggle_read_only
		);
	} else {
		return DrawOptionalComponent<Target, T>(
			target, label, tree,
			[&target](T& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<T>(), std::addressof(value)
				);
			},
			contents_read_only, toggle_read_only
		);
	}
}

template <typename Target, typename T>
bool DrawOptionalValue(Target& target, std::string_view label, FieldOptions options = {}) {
	return DrawOptionalComponent<Target, T>(target, label, false, [&](T& value) {
		return DrawValue(target.ctx, label, value, options);
	});
}

template <typename Target, typename T>
bool AddFeature(Target& target, std::string_view label) {
	auto before{ target.template Capture<T>() };

	if (before) {
		return false;
	}

	target.template SetLive<T>(T{});
	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, std::string{ "Add " } + std::string{ label }, std::move(before), std::move(after),
		true
	);

	return true;
}


template <typename Target>
bool DrawName(Target& target) {
	const std::string before{ target.GetName() };
	std::string value{ before };

	const bool changed{
		DrawPropertyRow(
			"Tag",
			[&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);
				return ImGui::InputText("##Tag", &value);
			}
		)
	};

	if (changed) {
		target.SetName(value);

		ScopedID target_scope{ target.Id() };
		const ImGuiID key{ ImGui::GetID("##NameEdit") };
		auto apply{ target.MakeNameApply() };

		TrackUndoableInteraction(
			target.ctx,
			key,
			"Rename Entity",
			true,
			[apply, before]() mutable {
				apply(before);
			},
			[apply, value]() mutable {
				apply(value);
			}
		);
	}

	if constexpr (requires { target.GetUUIDText(); }) {
		const std::string uuid{ target.GetUUIDText() };

		DrawPropertyRow(
			"UUID",
			[&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);
				std::string displayed{ uuid };
				ScopedDisabled read_only{ true };
				ImGui::InputText(
					"##UUID",
					&displayed,
					ImGuiInputTextFlags_ReadOnly
				);
				DrawTooltip("Read only entity identifier.");
				return false;
			}
		);
	}

	return changed;
}

} // namespace ptgn::editor::inspector
