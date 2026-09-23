#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;
class EditorContext;

enum class EditorSelectionMode {
	SceneHierarchy,
	Prefabs,
	Tiles
};

enum class SceneListTab {
	Scenes,
	ScreenEffects
};

enum class InspectorTab {
	Primary,
	ScreenEffect
};

struct SceneEntitySelection {
	std::string scene_key{};
	bool runtime{ false };
	std::optional<UUID> entity_uuid{};

	constexpr bool operator==(const SceneEntitySelection&) const = default;

	PTGN_REFLECT(SceneEntitySelection, scene_key, runtime, entity_uuid)
};

struct ScreenEffectSelection {
	ScreenEffectId id{ 0 };
	bool runtime{ false };

	constexpr bool operator==(const ScreenEffectSelection&) const = default;

	PTGN_REFLECT(ScreenEffectSelection, id, runtime)
};

struct EditorSelection {
	std::string selected_scene_key{};
	bool selected_scene_runtime{ false };
	std::vector<SceneEntitySelection> scene_entities{};

	std::optional<PrefabKey> selected_prefab{};
	SerializedEntityPath selected_prefab_entity_path{};
	std::optional<ScreenEffectSelection> selected_screen_effect{};

	EditorSelectionMode mode{ EditorSelectionMode::SceneHierarchy };
	SceneListTab scene_list_tab{ SceneListTab::Scenes };
	InspectorTab inspector_tab{ InspectorTab::Primary };

	void Clear();

	[[nodiscard]] bool HasEntitySelection(std::string_view scene_key, bool runtime) const;
	[[nodiscard]] std::optional<UUID> GetEntityUUID(std::string_view scene_key, bool runtime) const;
	void SetEntityUUID(std::string scene_key, bool runtime, std::optional<UUID> entity_uuid);
	void RemoveScene(std::string_view scene_key);
	void RenameScene(std::string_view old_key, std::string_view new_key);

	bool operator==(const EditorSelection&) const = default;
	bool HasSceneSelection() const;

	PTGN_REFLECT(
		EditorSelection,
		selected_scene_key,
		selected_scene_runtime,
		scene_entities,
		selected_prefab,
		selected_prefab_entity_path,
		selected_screen_effect,
		mode,
		scene_list_tab,
		inspector_tab
	)
};

[[nodiscard]] Scene* ResolveSelectedScene(EditorContext& ctx);
[[nodiscard]] const Scene* ResolveSelectedScene(const EditorContext& ctx);
[[nodiscard]] Entity ResolveSelectedEntity(EditorContext& ctx);

void ApplyEditorSelection(EditorContext& ctx, EditorSelection selection);

bool SetEditorSelection(
	EditorContext& ctx,
	EditorSelection selection,
	std::string label,
	bool allow_when_undo_disabled = false,
	bool transient = false
);

bool SetSceneHierarchyTab(EditorContext& ctx, EditorSelectionMode tab);
bool SetSceneListTab(EditorContext& ctx, SceneListTab tab);
bool SyncVisibleSceneListTab(EditorContext& ctx, SceneListTab tab);
bool SetInspectorTab(EditorContext& ctx, InspectorTab tab);
bool ClearSelectedScreenEffect(EditorContext& ctx, std::string label = "Deselect Screen Effect");

} // namespace editor

} // namespace ptgn
