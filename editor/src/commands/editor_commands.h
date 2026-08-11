#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <string_view>

#include "commands/component/add_component.h"
#include "commands/component/remove_component.h"
#include "commands/component/set_component.h"
#include "commands/undo_stack.h"
#include "editor/editor_selection.h"
#include "core/util/file.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;
class EditorContext;

class EditorCommands {
public:
	EditorCommands() = default;
	explicit EditorCommands(EditorContext* context);

	void Bind(EditorContext& context);

	Entity CreateEntity(std::string_view name);
	Entity RecordCreatedEntity(Entity entity, EditorSelection before_selection);
	Entity DuplicateEntity(Entity entity);
	void DeleteEntity(Entity entity);

	void RenameEntity(Entity entity, std::string_view new_name);
	void ReparentEntity(Entity child, Entity new_parent, bool preserve_world_transform);

	/// @brief Creates a prefab asset and selects its root.
	/// @return The created key, or an empty key if creation failed.
	[[nodiscard]] PrefabKey CreatePrefabAsset(Prefab prefab);

	/// @brief Deletes a prefab asset. Undo restores the complete asset and selection.
	[[nodiscard]] bool DeletePrefabAsset(const PrefabKey& key);

	/// @brief Renames a prefab asset while preserving its complete serialized hierarchy.
	[[nodiscard]] bool RenamePrefabAsset(
		const PrefabKey& old_key,
		const PrefabKey& new_key
	);

	/// @brief Copies a prefab to a new key and selects the duplicate.
	[[nodiscard]] PrefabKey DuplicatePrefabAsset(
		const PrefabKey& source_key,
		PrefabKey duplicate_key
	);

	/// @brief Adds a serialized child to a prefab entity.
	///
	/// parent_path is empty for the prefab root. The newly created child is selected.
	[[nodiscard]] bool AddPrefabChild(
		const PrefabKey& key,
		SerializedEntityPath parent_path,
		SerializedEntity child
	);

	/// @brief Duplicates a non-root serialized prefab entity beside the source.
	[[nodiscard]] bool DuplicatePrefabEntity(
		const PrefabKey& key,
		SerializedEntityPath entity_path
	);

	/// @brief Deletes a non-root serialized prefab entity and selects its parent.
	[[nodiscard]] bool DeletePrefabEntity(
		const PrefabKey& key,
		SerializedEntityPath entity_path
	);

	/// @brief Instantiates a prefab in a scene as one undoable entity creation.
	[[nodiscard]] Entity CreatePrefabInstance(
		Scene& scene,
		const PrefabKey& key
	);

	template <std::copy_constructible T>
	void SetComponentValue(Entity entity, const T& value) {
		if (!editor_ || !undo_stack_ || !entity) {
			return;
		}

		std::optional<T> before;
		if (const auto* current{ entity.template TryGet<T>() }) {
			before = *current;
		}

		undo_stack_->Execute(std::make_unique<SetComponentValueCommand<T>>(
			*editor_,
			MakeEntityReference(entity),
			std::move(before),
			value
		));
	}

	template <std::copy_constructible T>
	void RemoveComponent(Entity entity) {
		if (!editor_ || !undo_stack_ || !entity) {
			return;
		}

		const auto* component{ entity.template TryGet<T>() };
		if (!component) {
			return;
		}

		undo_stack_->Execute(std::make_unique<RemoveComponentCommand<T>>(
			*editor_,
			MakeEntityReference(entity),
			*component
		));
	}

	template <std::copy_constructible T>
	void AddComponent(Entity entity, const T& component) {
		if (!editor_ || !undo_stack_ || !entity || entity.template Has<T>()) {
			return;
		}

		undo_stack_->Execute(std::make_unique<AddComponentCommand<T>>(
			*editor_,
			MakeEntityReference(entity),
			component
		));
	}

	void SaveScene(const path& path);
	void LoadScene(const path& path);

private:
	Editor* editor_{ nullptr };
	UndoStack* undo_stack_{ nullptr };
	EditorContext* context_{ nullptr };
};

} // namespace editor

} // namespace ptgn
