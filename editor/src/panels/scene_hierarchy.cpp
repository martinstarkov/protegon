#include "panels/scene_hierarchy.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "commands/editor_commands.h"
#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/util/string.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_icons.h"
#include "editor/paint/paint_editor.h"
#include "editor/renamable_item.h"
#include "panels/entity_hierarchy.h"
#include "panels/scene_list.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/group_stack.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/edge_detection.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/fx/inverse_color.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/fx/sharpen.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/chunk.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"

namespace ptgn::editor {

namespace {

constexpr const char* kEntityDragDropPayload{ "PTGN_HIERARCHY_ENTITY" };
constexpr const char* kPrefabAssetDragDropPayload{ "PTGN_PREFAB_ASSET" };
constexpr ImU32 kPrefabLinkHighlightColor{ IM_COL32(239, 186, 59, 255) };

template <typename Range>
[[nodiscard]] std::string NextDefaultGroupName(const Range& groups) {
	for (std::size_t index{ 1 };; ++index) {
		std::string candidate{ "Group " + std::to_string(index) };
		if (!std::ranges::contains(groups, candidate)) {
			return candidate;
		}
	}
}

using HierarchyCondition = bool (*)(Entity);

enum class ParentAssignmentPolicy {
	SameParentOnly,
	RootOnly
};

struct ParentAssignmentRule {
	HierarchyCondition condition{};
	ParentAssignmentPolicy policy{};
	std::string_view reason{};
};

struct ChildAcceptanceRule {
	HierarchyCondition condition{};
	std::string_view reason{};
};

struct DeletionRule {
	HierarchyCondition condition{};
	std::string_view reason{};
};

bool IsManagedButtonVisual(Entity entity) {
	if (!entity || !ptgn::HasParent(entity)) {
		return false;
	}

	Entity parent{ GetParent(entity) };

	if (entity.HasAny<
			ButtonBackgroundVisuals, ButtonBorderVisuals, ButtonSpriteVisuals,
			ButtonTextVisuals>() &&
		parent.Has<::ptgn::impl::ButtonData>()) {
		return true;
	}

	if (entity.HasAny<
			::ptgn::impl::SliderThumbData, ::ptgn::impl::SliderTrackData,
			::ptgn::impl::SliderValueTextData>() &&
		parent.Has<::ptgn::impl::SliderData>()) {
		return true;
	}

	if (entity.HasAny<
			::ptgn::impl::SliderTrackBackgroundData, ::ptgn::impl::SliderTrackBorderData,
			::ptgn::impl::SliderTrackSpriteData>() &&
		parent.Has<::ptgn::impl::SliderTrackData>()) {
		return true;
	}

	if (entity.Has<::ptgn::impl::DialoguePart>() && parent.Has<::ptgn::impl::DialogueData>()) {
		return true;
	}

	return false;
}

bool IsManagedInteractiveShape(Entity entity) {
	return entity && entity.Has<::ptgn::impl::InteractiveTag>();
}

bool IsRuntimeWorldContent(Entity entity) {
	// World instances still exist in the ECS for rendering, interaction and
	// physics. Only their authoring representation is hidden from the editor.
	return entity && entity.HasScene() && entity.GetScene().IsRuntime() &&
		   entity.GetScene().ctx().chunks.IsManagedRuntimeEntity(entity);
}

bool IsCameraEntity(Entity entity) {
	return entity.Has<::ptgn::impl::CameraData>();
}

bool IsPrimarySceneCamera(Entity entity) {
	return entity && entity == entity.GetScene().GetCamera();
}

bool IsFixedSceneCamera(Entity entity) {
	return entity && entity == entity.GetScene().GetFixedCamera();
}

bool IsPrimarySceneRenderTarget(Entity entity) {
	return entity && entity == entity.GetScene().GetRenderTarget();
}

/// @brief Restrictions on which entities may be deleted through the hierarchy.
constexpr std::array kDeletionRules{
	DeletionRule{
		.condition = IsPrimarySceneRenderTarget,
		.reason{ "Scene render target cannot be deleted" },
	},
	DeletionRule{
		.condition = IsPrimarySceneCamera,
		.reason{ "Primary camera cannot be deleted" },
	},
	DeletionRule{
		.condition = IsFixedSceneCamera,
		.reason{ "Scene fixed camera cannot be deleted" },
	},
};

/// @brief Restrictions on which parent an entity may be assigned to.
constexpr std::array kParentAssignmentRules{
	ParentAssignmentRule{
		.condition = IsManagedInteractiveShape,
		.policy	   = ParentAssignmentPolicy::SameParentOnly,
		.reason{ "Interactive hit area cannot be reparented" },
	},
	ParentAssignmentRule{
		.condition = IsManagedButtonVisual,
		.policy	   = ParentAssignmentPolicy::SameParentOnly,
		.reason{ "Button part cannot be reparented" },
	},
	ParentAssignmentRule{
		.condition = IsCameraEntity,
		.policy	   = ParentAssignmentPolicy::RootOnly,
		.reason{ "Camera cannot have parent" },
	},
	ParentAssignmentRule{
		.condition = IsPrimarySceneRenderTarget,
		.policy	   = ParentAssignmentPolicy::RootOnly,
		.reason{ "Primary render target cannot have parent" },
	},
};

/// @brief Restrictions on which entities may receive children.
constexpr std::array kChildAcceptanceRules{
	ChildAcceptanceRule{
		.condition = IsCameraEntity,
		.reason{ "Camera cannot have children" },
	},
	ChildAcceptanceRule{
		.condition = IsPrimarySceneRenderTarget,
		.reason{ "Primary render target cannot have children" },
	},
	ChildAcceptanceRule{
		.condition = IsPaintGenerator,
		.reason{ "Paint generator cannot have child entities" },
	},
};

bool IsParentAssignmentAllowed(Entity entity, Entity new_parent, ParentAssignmentPolicy policy) {
	switch (policy) {
		case ParentAssignmentPolicy::SameParentOnly: {
			Entity current_parent{ ptgn::HasParent(entity) ? GetParent(entity) : Entity{} };

			return current_parent == new_parent;
		}

		case ParentAssignmentPolicy::RootOnly: return !new_parent;
	}

	return false;
}

std::optional<std::string_view> GetDeletionLockReason(Entity entity) {
	if (!entity) {
		return "Invalid entity";
	}

	for (const auto& rule : kDeletionRules) {
		if (rule.condition(entity)) {
			return rule.reason;
		}
	}

	return std::nullopt;
}

std::optional<std::string_view> GetDuplicationLockReason(Entity entity) {
	if (auto reason{ GetDeletionLockReason(entity) }) {
		return reason;
	}

	if (IsManagedInteractiveShape(entity)) {
		return "Interactive hit area cannot be duplicated independently";
	}
	if (IsManagedButtonVisual(entity)) {
		return "Button part cannot be duplicated independently";
	}

	return std::nullopt;
}

std::optional<std::string_view> GetParentAssignmentLockReason(Entity entity, Entity new_parent) {
	if (!entity) {
		return "Invalid entity";
	}

	for (const auto& rule : kParentAssignmentRules) {
		if (rule.condition(entity) && !IsParentAssignmentAllowed(entity, new_parent, rule.policy)) {
			return rule.reason;
		}
	}

	return std::nullopt;
}

std::optional<std::string_view> GetChildAcceptanceLockReason(Entity parent) {
	if (!parent) {
		return std::nullopt;
	}

	for (const auto& rule : kChildAcceptanceRules) {
		if (rule.condition(parent)) {
			return rule.reason;
		}
	}

	return std::nullopt;
}

std::optional<std::string_view> GetHierarchyRestrictionReason(Entity entity) {
	if (!entity) {
		return std::nullopt;
	}

	for (const auto& rule : kParentAssignmentRules) {
		if (rule.condition(entity)) {
			return rule.reason;
		}
	}

	for (const auto& rule : kChildAcceptanceRules) {
		if (rule.condition(entity)) {
			return rule.reason;
		}
	}

	return std::nullopt;
}

struct PendingHierarchyDrop {
	Entity entity{};
	Entity parent{};

	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(entity);
	}
};

bool EntityOrDescendantMatchesFilter(
	Entity entity, std::string_view filter_text, bool show_managed_ui_parts,
	std::size_t recursion_depth = 0
) {
	if (!entity) {
		return false;
	}

	PTGN_ASSERT(
		recursion_depth <= kMaxParentDepth,
		"Maximum parent depth exceeded while filtering entities. "
		"This likely indicates a cycle in the entity hierarchy"
	);

	// Interactive hit areas are always managed through the Interaction inspector.
	if (IsManagedInteractiveShape(entity) || IsRuntimeWorldContent(entity)) {
		return false;
	}

	// Managed UI parts are implementation details of their owning control. When they are hidden,
	// exclude the entire managed branch from both hierarchy drawing and filter matching.
	if (!show_managed_ui_parts && IsManagedButtonVisual(entity)) {
		return false;
	}

	if (hierarchy::MatchesFilter(entity, filter_text)) {
		return true;
	}

	if (!HasChildren(entity)) {
		return false;
	}

	for (Entity child : GetChildren(entity)) {
		if (EntityOrDescendantMatchesFilter(
				child, filter_text, show_managed_ui_parts, recursion_depth + 1
			)) {
			return true;
		}
	}

	return false;
}

bool IsSameOrDescendant(Entity entity, Entity potential_ancestor) {
	for (std::size_t depth{ 0 }; depth <= kMaxParentDepth; ++depth) {
		if (!entity) {
			return false;
		}

		if (entity == potential_ancestor) {
			return true;
		}

		if (!ptgn::HasParent(entity)) {
			return false;
		}

		entity = GetParent(entity);
	}

	PTGN_ASSERT(
		false, "Maximum parent depth exceeded while checking an entity hierarchy. "
			   "This likely indicates a cycle"
	);
	return true;
}

bool CanReparent(Entity entity, Entity parent) {
	if (!entity) {
		return false;
	}

	// Group stacks accept only direct Group children. Groups themselves remain
	// valid as standalone parents anywhere else in the scene hierarchy.
	if (parent && parent.Has<GroupStackData>() && !entity.Has<Group>()) {
		return false;
	}

	if (GetPrefabInstance(entity) && GetPrefabInstanceRoot(entity) != entity) {
		return false;
	}
	if (parent && GetPrefabInstance(parent)) {
		return false;
	}

	if (parent && IsSameOrDescendant(parent, entity)) {
		return false;
	}

	if (GetParentAssignmentLockReason(entity, parent).has_value()) {
		return false;
	}

	if (GetChildAcceptanceLockReason(parent).has_value()) {
		return false;
	}

	return true;
}

Entity GetDraggedEntity(Scene& scene) {
	const ImGuiPayload* payload{ ImGui::GetDragDropPayload() };

	if (!payload || !payload->IsDataType(kEntityDragDropPayload) ||
		payload->DataSize != sizeof(UUID)) {
		return {};
	}

	auto uuid{ *static_cast<const UUID*>(payload->Data) };
	return scene.GetEntity(uuid);
}

Entity AcceptDraggedEntity(Scene& scene) {
	const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload(kEntityDragDropPayload) };

	if (!payload || payload->DataSize != sizeof(UUID)) {
		return {};
	}

	auto uuid{ *static_cast<const UUID*>(payload->Data) };
	return scene.GetEntity(uuid);
}

void ApplyHierarchyDrop(EditorContext& ctx, const PendingHierarchyDrop& drop) {
	if (!drop || !CanReparent(drop.entity, drop.parent)) {
		return;
	}

	Entity previous_parent{ ptgn::HasParent(drop.entity) ? GetParent(drop.entity) : Entity{} };

	if (previous_parent == drop.parent) {
		return;
	}

	ctx.commands.ReparentEntity(drop.entity, drop.parent, true);
}

void DrawRestrictionReasons(std::initializer_list<std::optional<std::string_view>> reasons) {
	std::vector<std::string_view> drawn_reasons;

	for (const auto& reason : reasons) {
		if (!reason.has_value() || reason->empty()) {
			continue;
		}

		if (std::ranges::find(drawn_reasons, reason.value()) != drawn_reasons.end()) {
			continue;
		}

		ImGui::TextDisabled("%.*s", static_cast<int>(reason->size()), reason->data());
		drawn_reasons.emplace_back(reason.value());
	}
}

constexpr V2_float kDefaultShapeSize{ 100, 100 };
constexpr float kDefaultShapeRadius{ 50.0f };
constexpr V2_float kDefaultInteractiveSpriteSize{ 100, 100 };

constexpr V2_float kDefaultUIButtonSize{ 180, 50 };
constexpr V2_float kDefaultUIDialogueSize{ 640, 160 };

Prefab CreatePrefabFromEntity(const AssetManager& assets, const path& project_root, Entity entity);

struct CreateMenuContext {
	EditorContext& ctx;
	Scene& scene;
	Entity parent{};
	Entity& selected_entity;

	[[nodiscard]] bool IsCreatingChild() const {
		return static_cast<bool>(parent);
	}
};

using CreateEntityFunction = Entity (*)(EntityCreationContext);

[[nodiscard]] std::string GetPrefabDisplayName(const PrefabKey& key);

// Default labels are unique within the scene or owning prefab definition.
template <typename Exists>
[[nodiscard]] std::string NextNumberedName(std::string_view archetype, Exists&& exists) {
	std::string stem{ MakePrefabSlug(archetype) };
	for (std::size_t index{ 1 };; ++index) {
		std::string candidate{ stem + std::to_string(index) };
		if (!exists(candidate)) {
			return candidate;
		}
	}
}

[[nodiscard]] std::string NextSceneEntityName(Scene& scene, std::string_view archetype) {
	return NextNumberedName(archetype, [&](std::string_view candidate) {
		for (Entity entity : scene.Entities()) {
			if (entity && entity.Has<Tag>() && entity.Get<Tag>().value == candidate) {
				return true;
			}
		}
		return false;
	});
}

[[nodiscard]] bool PrefabContainsName(const SerializedEntity& entity, std::string_view name) {
	if (entity.tag == name) {
		return true;
	}
	for (const SerializedEntity& child : entity.children) {
		if (PrefabContainsName(child, name)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::string NextPrefabEntityName(
	AssetManager& assets, const PrefabKey& key, std::string_view archetype
) {
	::ptgn::impl::AssetAccessor accessor{ assets };
	if (!accessor.Has<Prefab>(key)) {
		return NextNumberedName(archetype, [](std::string_view) { return false; });
	}
	const Prefab& prefab{ accessor.Get<Prefab>(key).get() };
	return NextNumberedName(archetype, [&](std::string_view candidate) {
		return PrefabContainsName(prefab.root, candidate);
	});
}

[[nodiscard]] PrefabKey MakeNumberedPrefabKey(
	const AssetManager& assets, const path& project_root, std::string_view archetype
) {
	std::string name{ NextNumberedName(archetype, [&](std::string_view candidate) {
		PrefabKey key{ MakePrefabKey(candidate) };
		return assets.Has(key) || assets.HasCatalogAsset(key) ||
			   FileExists(project_root / GetPrefabSourcePath(key));
	}) };
	return MakePrefabKey(name);
}

void SetCreatedEntityTag(Entity entity, std::string_view tag) {
	if (entity && entity.Has<Tag>()) {
		entity.Get<Tag>().value = tag;
	}
}

void FinalizeCreatedEntity(
	CreateMenuContext& context, Entity created, std::string_view archetype = {}
) {
	if (!created) {
		return;
	}

	if (!archetype.empty()) {
		SetCreatedEntityTag(created, NextSceneEntityName(context.scene, archetype));
	}

	if (context.parent) {
		// This is a final safeguard for entities such as cameras that are
		// not allowed to become children.
		if (!CanReparent(created, context.parent)) {
			PTGN_WARN(
				"Could not create entity as a child of ", context.parent.Get<Tag>().value,
				" because the hierarchy relationship is restricted"
			);
			created.Destroy();
			return;
		}

		SetParent(created, context.parent);
	}

	context.selected_entity =
		context.ctx.commands.RecordCreatedEntity(created, context.ctx.local.selection);
}

// Prefab children are embedded entities, not independent prefab assets. Rename their
// serialized Tag and persist the complete asset so linked scene instances are updated.
[[nodiscard]] bool RenameEmbeddedPrefabEntity(
	EditorContext& ctx, const PrefabKey& key, const SerializedEntityPath& entity_path,
	std::string_view name
) {
	if (entity_path.empty() || name.empty()) {
		return false;
	}

	auto project_root{ ctx.editor.GetProjectRoot() };
	if (!project_root.has_value()) {
		return false;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	if (!assets.EnsurePrefabResident(key)) {
		return false;
	}

	Prefab before{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(key).get() };
	Prefab after{ before };
	SerializedEntity* child{ ResolveSerializedEntity(after.root, entity_path) };
	if (!child || child->tag == name) {
		return false;
	}

	child->tag = std::string{ name };

	auto apply = [editor = &ctx.editor, project_root = *project_root, key](Prefab prefab) {
		path source_path{ GetPrefabSourcePath(key) };
		editor->GetAssetManager().SavePrefab(
			std::move(prefab), project_root / source_path, source_path
		);

		for (const auto& scene : editor->GetSceneManager().GetScenes()) {
			if (scene && !scene->IsRuntime()) {
				(void)SyncPrefabInstances(*scene, key);
			}
		}
	};

	apply(after);
	ctx.undo.PushApplied(
		"Rename Prefab Entity", [apply, before]() { apply(before); },
		[apply, after]() { apply(after); }
	);
	ctx.local.state.is_dirty = true;
	return true;
}

struct PrefabArchetypeMenuContext {
	EditorContext& ctx;
	Scene& scene;
	AssetManager& assets;
	const path& project_root;
	std::string_view group;

	[[nodiscard]] bool IsCreatingChild() const {
		return false;
	}
};

struct PrefabChildMenuContext {
	EditorContext& ctx;
	Scene& scene;
	const PrefabKey& key;
	std::optional<SerializedEntity>& pending_child;

	[[nodiscard]] bool IsCreatingChild() const {
		return true;
	}
};

void FinalizeCreatedEntity(
	PrefabArchetypeMenuContext& context, Entity created, std::string_view archetype
) {
	if (!created) {
		return;
	}

	// Archetypes are built in a real scene so factories can safely access scene services.
	// The scene entity is temporary; only the serialized prefab definition is retained.
	PrefabKey prefab_key{ MakeNumberedPrefabKey(context.assets, context.project_root, archetype) };
	Prefab prefab{ CapturePrefab(created, prefab_key, true) };
	prefab.root.tag = GetPrefabDisplayName(prefab_key);
	created.Destroy();
	context.scene.Refresh();

	PrefabKey key{ context.ctx.commands.CreatePrefabAsset(std::move(prefab)) };
	if (key.value.empty()) {
		return;
	}

	if (context.group != "Ungrouped") {
		std::size_t after_creation{ context.ctx.undo.Cursor() };
		context.ctx.editor.GetPaintEditor().MovePrefabToGroup(
			context.ctx, key, std::string{ context.group }
		);
		if (context.ctx.undo.Cursor() == after_creation + 1) {
			(void)context.ctx.undo.MergeLastAppliedCommands("Create Prefab");
		}
	}

	context.ctx.editor.GetSceneHierarchyPanel().SetSelectedPrefab(key);
}

void FinalizeCreatedEntity(
	PrefabChildMenuContext& context, Entity created, std::string_view archetype
) {
	if (!created) {
		return;
	}

	context.pending_child = CapturePrefab(created, context.key, true).root;
	context.pending_child->tag =
		NextPrefabEntityName(context.ctx.editor.GetAssetManager(), context.key, archetype);
	created.Destroy();
	context.scene.Refresh();
}

void DrawCreateMenuItem(
	PrefabArchetypeMenuContext& context, const char* label, CreateEntityFunction create, bool = true
) {
	if (!ImGui::MenuItem(label)) {
		return;
	}

	FinalizeCreatedEntity(context, create(context.scene), label);
}

void DrawCreateMenuItem(
	PrefabChildMenuContext& context, const char* label, CreateEntityFunction create,
	bool can_create_as_child = true
) {
	if (!can_create_as_child || !ImGui::MenuItem(label)) {
		return;
	}

	FinalizeCreatedEntity(context, create(context.scene), label);
}

void DrawCreateMenuItem(
	CreateMenuContext& context, const char* label, CreateEntityFunction create,
	bool can_create_as_child = true
) {
	if (context.IsCreatingChild() && !can_create_as_child) {
		return;
	}

	if (ImGui::MenuItem(label)) {
		FinalizeCreatedEntity(context, create(context.scene), label);
	}
}

Entity CreateDefaultSprite(EntityCreationContext creation) {
	return CreateSprite(creation);
}

Entity CreateDefaultSpriteStack(EntityCreationContext creation) {
	return CreateSpriteStack(creation);
}

Entity CreateDefaultAnimation(EntityCreationContext creation) {
	return CreateAnimation(creation);
}

Entity CreateDefaultParticleEmitter(EntityCreationContext creation) {
	return CreateParticleEmitter(creation, {}, {}, true);
}

Entity CreateDefaultLight(EntityCreationContext creation) {
	return CreateLight(creation);
}

Entity CreateDefaultRenderTarget(EntityCreationContext creation) {
	return CreateRenderTarget(creation);
}

Entity CreateDefaultCamera(EntityCreationContext creation) {
	return CreateCamera(creation);
}

Entity CreateDefaultCustomShader(EntityCreationContext creation) {
	return CreateCustomShader(creation);
}

Entity CreateDefaultRect(EntityCreationContext creation) {
	return CreateRect(creation, {}, kDefaultShapeSize, color::White);
}

Entity CreateDefaultCircle(EntityCreationContext creation) {
	return CreateCircle(creation, {}, kDefaultShapeRadius, color::White);
}

Entity CreateDefaultLine(EntityCreationContext creation) {
	return CreateLine(creation, {}, { -100, -100 }, { 100, 100 }, color::White);
}

Entity CreateDefaultPolygon(EntityCreationContext creation) {
	return CreatePolygon(
		creation, {},
		{
			{ 0, -50 },
			{ 47, -15 },
			{ 29, 40 },
			{ -29, 40 },
			{ -47, -15 },
		},
		color::White
	);
}

Entity CreateDefaultEllipse(EntityCreationContext creation) {
	return CreateEllipse(
		creation, {}, { kDefaultShapeRadius * 2, kDefaultShapeRadius }, color::White
	);
}

Entity CreateDefaultArc(EntityCreationContext creation) {
	return CreateArc(creation, {}, kDefaultShapeRadius, 0.0f, 90.0f, true, color::White);
}

Entity CreateDefaultRoundedRect(EntityCreationContext creation) {
	return CreateRoundedRect(creation, {}, kDefaultShapeSize, 10.0f, color::White);
}

Entity CreateDefaultTriangle(EntityCreationContext creation) {
	return CreateTriangle(creation, {}, { -100, 50 }, { 0, -50 }, { 100, 50 }, color::White);
}

Entity CreateDefaultCapsule(EntityCreationContext creation) {
	return CreateCapsule(
		creation, {}, { -100, -100 }, { 100, 100 }, kDefaultShapeRadius, color::White
	);
}

enum class InteractivePreset {
	Interactive,
	Draggable,
	Dropzone
};

void ApplyInteractivePreset(Entity entity, InteractivePreset preset) {
	switch (preset) {
		case InteractivePreset::Interactive: break;
		case InteractivePreset::Draggable:	 SetDraggable(entity); break;
		case InteractivePreset::Dropzone:	 SetDropzone(entity); break;
	}
}

template <InteractivePreset Preset>
Entity CreateDefaultInteractiveRect(EntityCreationContext creation) {
	auto entity{ CreateRect(creation, {}, kDefaultShapeSize, color::White) };
	auto shape{ creation.CreateEntity() };
	shape.Add<Rect>(kDefaultShapeSize);
	AddInteractiveShape(entity, shape);
	ApplyInteractivePreset(entity, Preset);
	return entity;
}

template <InteractivePreset Preset>
Entity CreateDefaultInteractiveCircle(EntityCreationContext creation) {
	auto entity{ CreateCircle(creation, {}, kDefaultShapeRadius, color::White) };
	auto shape{ creation.CreateEntity() };
	shape.Add<Circle>(kDefaultShapeRadius);
	AddInteractiveShape(entity, shape);
	ApplyInteractivePreset(entity, Preset);
	return entity;
}

template <InteractivePreset Preset>
Entity CreateDefaultInteractiveSprite(EntityCreationContext creation) {
	auto entity{ CreateSprite(creation) };
	auto shape{ creation.CreateEntity() };
	shape.Add<Rect>(kDefaultInteractiveSpriteSize);
	AddInteractiveShape(entity, shape);
	ApplyInteractivePreset(entity, Preset);
	return entity;
}

void ConfigureDefaultButton(Button button, std::string_view text) {
	button.Background().Color(color::Black.WithAlpha(180));
	button.Text().Content(text).Color(color::White);
}

Entity CreateDefaultUIButton(EntityCreationContext creation) {
	auto button{ CreateButton(creation, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(button, "Button");
	SetCreatedEntityTag(button, "Button");
	return button;
}

Entity CreateDefaultUIToggleButton(EntityCreationContext creation) {
	auto button{ CreateToggleButton(creation, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(button, "Toggle");
	SetCreatedEntityTag(button, "Toggle Button");
	return button;
}

Entity CreateDefaultUIToggleGroup(EntityCreationContext creation) {
	auto group{ CreateToggleButtonGroup(creation) };
	SetCreatedEntityTag(group, "Toggle Group");
	return group;
}

Entity CreateDefaultGroupStack(EntityCreationContext creation) {
	GroupStack stack{ CreateGroupStack(creation) };
	[[maybe_unused]] GroupEntity initial{ stack.AddGroup("Main") };
	return stack;
}

Entity CreateDefaultGroupEntity(EntityCreationContext creation) {
	return CreateGroup(creation, "Group");
}

Entity CreateDefaultUISlider(EntityCreationContext creation) {
	auto slider{ CreateSlider(
		creation, Line{ {}, V2_float{ 200.0f, 0.0f } }, V2_float{ 24.0f, 24.0f }, Origin::Center,
		0.5f
	) };
	SetCreatedEntityTag(slider, "Slider");
	return slider;
}

Entity CreateDefaultUIDropdown(EntityCreationContext creation) {
	auto dropdown{ CreateDropdown(creation, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(dropdown, "Dropdown");

	auto first_item{ dropdown.AddItem("Item 1") };
	auto second_item{ dropdown.AddItem("Item 2") };
	ConfigureDefaultButton(first_item, "Item 1");
	ConfigureDefaultButton(second_item, "Item 2");

	SetCreatedEntityTag(dropdown, "Dropdown");
	return dropdown;
}

Entity CreateDefaultUIText(EntityCreationContext creation) {
	auto text{ CreateText(creation, {}, "Default Text", color::White) };
	SetCreatedEntityTag(text, "Text");
	return text;
}

std::string GetUniqueTooltipName(EntityCreationContext creation) {
	std::string name{ "Tooltip" };
	std::size_t suffix{ 2 };

	while (creation.HasScene() && Tooltip::Get(*creation.GetScene(), name).has_value()) {
		name = "Tooltip " + std::to_string(suffix++);
	}

	return name;
}

Entity CreateDefaultUITooltip(EntityCreationContext creation) {
	auto name{ GetUniqueTooltipName(creation) };

	TooltipProperties properties;
	properties.content = "Tooltip";

	auto tooltip{ CreateTooltip(creation, name, properties) };
	SetCreatedEntityTag(tooltip, name);
	return tooltip;
}

Entity CreateDefaultUIDialogueBox(EntityCreationContext creation) {
	DialogueDesc desc;
	desc.box_size = kDefaultUIDialogueSize;

	auto dialogue{ CreateDialogueBox(creation, {}, desc) };
	SetCreatedEntityTag(dialogue, "Dialogue");
	return dialogue;
}

template <typename MenuContext>
void DrawEffectsCreateMenu(MenuContext& context) {
	if (!ImGui::BeginMenu("Effects")) {
		return;
	}

	std::vector<const ::ptgn::impl::RegisteredEffect*> effects;
	for (const auto& effect : ::ptgn::impl::EffectRegistry::Entries()) {
		effects.emplace_back(&effect);
	}

	std::ranges::sort(effects, {}, [](const auto* effect) { return effect->display_name; });

	bool has_creatable_effect{ false };

	for (const auto* effect : effects) {
		if (!effect || !effect->create || !effect->make_default) {
			continue;
		}

		has_creatable_effect = true;

		std::string label{ effect->display_name };
		if (effect->hdr) {
			label += " [HDR]";
		}

		if (!ImGui::MenuItem(label.c_str())) {
			continue;
		}

		Entity entity{ context.scene.CreateEntity() };
		entity.Add<Tag>(effect->display_name + " Entity");
		effect->create(entity, effect->make_default());
		FinalizeCreatedEntity(context, entity, effect->display_name);
	}

	if (!has_creatable_effect) {
		ImGui::TextDisabled("No creatable effects are registered.");
	}

	ImGui::EndMenu();
}

template <typename MenuContext>
void DrawShapesCreateMenu(MenuContext& context) {
	if (!ImGui::BeginMenu("Shapes")) {
		return;
	}

	DrawCreateMenuItem(context, "Rect", CreateDefaultRect);
	DrawCreateMenuItem(context, "Circle", CreateDefaultCircle);
	DrawCreateMenuItem(context, "Line", CreateDefaultLine);
	DrawCreateMenuItem(context, "Polygon", CreateDefaultPolygon);
	DrawCreateMenuItem(context, "Ellipse", CreateDefaultEllipse);
	DrawCreateMenuItem(context, "Arc", CreateDefaultArc);
	DrawCreateMenuItem(context, "Rounded Rect", CreateDefaultRoundedRect);
	DrawCreateMenuItem(context, "Triangle", CreateDefaultTriangle);
	DrawCreateMenuItem(context, "Capsule", CreateDefaultCapsule);

	ImGui::EndMenu();
}

template <typename MenuContext>
void DrawInteractivePresetItems(MenuContext& context, InteractivePreset preset) {
	switch (preset) {
		case InteractivePreset::Interactive:
			DrawCreateMenuItem(
				context, "Rect", CreateDefaultInteractiveRect<InteractivePreset::Interactive>
			);
			DrawCreateMenuItem(
				context, "Circle", CreateDefaultInteractiveCircle<InteractivePreset::Interactive>
			);
			DrawCreateMenuItem(
				context, "Sprite", CreateDefaultInteractiveSprite<InteractivePreset::Interactive>
			);
			break;

		case InteractivePreset::Draggable:
			DrawCreateMenuItem(
				context, "Rect", CreateDefaultInteractiveRect<InteractivePreset::Draggable>
			);
			DrawCreateMenuItem(
				context, "Circle", CreateDefaultInteractiveCircle<InteractivePreset::Draggable>
			);
			DrawCreateMenuItem(
				context, "Sprite", CreateDefaultInteractiveSprite<InteractivePreset::Draggable>
			);
			break;

		case InteractivePreset::Dropzone:
			DrawCreateMenuItem(
				context, "Rect", CreateDefaultInteractiveRect<InteractivePreset::Dropzone>
			);
			DrawCreateMenuItem(
				context, "Circle", CreateDefaultInteractiveCircle<InteractivePreset::Dropzone>
			);
			DrawCreateMenuItem(
				context, "Sprite", CreateDefaultInteractiveSprite<InteractivePreset::Dropzone>
			);
			break;
	}
}

template <typename MenuContext>
void DrawInteractiveCreateMenu(MenuContext& context) {
	if (!ImGui::BeginMenu("Interactive")) {
		return;
	}

	DrawInteractivePresetItems(context, InteractivePreset::Interactive);

	if (ImGui::BeginMenu("Draggable")) {
		DrawInteractivePresetItems(context, InteractivePreset::Draggable);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Dropzone")) {
		DrawInteractivePresetItems(context, InteractivePreset::Dropzone);
		ImGui::EndMenu();
	}

	ImGui::EndMenu();
}

template <typename MenuContext>
void DrawUICreateMenu(MenuContext& context) {
	if (!ImGui::BeginMenu("UI")) {
		return;
	}

	DrawCreateMenuItem(context, "Text", CreateDefaultUIText);
	DrawCreateMenuItem(context, "Button", CreateDefaultUIButton);
	DrawCreateMenuItem(context, "Toggle Button", CreateDefaultUIToggleButton);
	DrawCreateMenuItem(context, "Slider", CreateDefaultUISlider);
	DrawCreateMenuItem(context, "Dropdown", CreateDefaultUIDropdown);
	DrawCreateMenuItem(context, "Tooltip", CreateDefaultUITooltip);
	DrawCreateMenuItem(context, "Dialogue", CreateDefaultUIDialogueBox);
	DrawCreateMenuItem(context, "Toggle Group", CreateDefaultUIToggleGroup);

	ImGui::EndMenu();
}

PrefabKey MakeUniquePrefabKey(
	const AssetManager& assets, const path& project_root, std::string_view display_name
) {
	std::string base{ MakePrefabKey(display_name).value };
	PrefabKey key{ base };
	std::size_t suffix{ 2 };

	while (assets.Has(key) || assets.HasCatalogAsset(key) ||
		   FileExists(project_root / GetPrefabSourcePath(key))) {
		key = PrefabKey{ base + "_" + std::to_string(suffix++) };
	}

	return key;
}

void AddDefaultPrefabComponent(SerializedEntity& entity, const RegisteredComponent* component) {
	if (!component || !IsPrefabComponentSupported(*component)) {
		return;
	}

	if (component->is_empty) {
		std::string name{ component->name };

		if (!std::ranges::contains(entity.tags, name)) {
			entity.tags.emplace_back(name);
		}

		return;
	}

	auto default_value{ component->MakeDefaultJson() };
	if (!default_value) {
		return;
	}

	json value{ std::move(*default_value) };

	if (value.is_null()) {
		value = json::object();
	}

	entity.components.insert_or_assign(std::string{ component->name }, std::move(value));
}

void AddDefaultPrefabSpatialComponents(SerializedEntity& entity) {
	AddDefaultPrefabComponent(entity, ComponentRegistry::Find<Transform>());

	AddDefaultPrefabComponent(entity, ComponentRegistry::Find<Depth>());
}

[[nodiscard]] std::string GetPrefabDisplayName(const PrefabKey& key) {
	std::string name{ key.value };

	if (name.starts_with(kPrefabKeyPrefix)) {
		name.erase(0, kPrefabKeyPrefix.size());
	}

	return name;
}

[[nodiscard]] bool IsPrefabKeyAvailable(
	const AssetManager& assets, const path& project_root, const PrefabKey& current_key,
	const PrefabKey& candidate_key
) {
	if (candidate_key == current_key) {
		return true;
	}

	if (assets.Has(candidate_key) || assets.HasCatalogAsset(candidate_key)) {
		return false;
	}

	return !FileExists(project_root / GetPrefabSourcePath(candidate_key));
}

Prefab CreateBlankPrefab(const AssetManager& assets, const path& project_root) {
	Prefab prefab;

	prefab.key = MakeNumberedPrefabKey(assets, project_root, "Prefab");

	prefab.root.tag = GetPrefabDisplayName(prefab.key);

	AddDefaultPrefabSpatialComponents(prefab.root);

	return prefab;
}

Prefab CreatePrefabFromEntity(const AssetManager& assets, const path& project_root, Entity entity) {
	std::string name{ entity.Has<Tag>() ? entity.Get<Tag>().value : std::string{ "Prefab" } };

	auto key{ MakeUniquePrefabKey(assets, project_root, name) };

	return CapturePrefab(entity, key, true);
}

void DrawPrefabCreateMenu(CreateMenuContext& context) {
	if (!ImGui::BeginMenu("Prefab")) {
		return;
	}

	auto& assets{ context.scene.ctx().asset };
	auto& paint{ context.ctx.editor.GetPaintEditor() };
	auto keys{ assets.GetPrefabKeys() };
	std::ranges::sort(keys, {}, [](const PrefabKey& key) { return key.value; });
	auto groups{ paint.GetPrefabGroups(context.ctx) };

	for (const std::string& group : groups) {
		bool has_prefabs{ std::ranges::any_of(keys, [&](const PrefabKey& key) {
			return paint.GetPrefabGroup(context.ctx, key) == group;
		}) };
		if (!has_prefabs || !ImGui::BeginMenu(group.c_str())) {
			continue;
		}

		for (const PrefabKey& key : keys) {
			if (paint.GetPrefabGroup(context.ctx, key) != group) {
				continue;
			}
			std::string label{ GetPrefabDisplayName(key) };
			if (ImGui::MenuItem(label.c_str())) {
				FinalizeCreatedEntity(context, context.scene.CreatePrefab(key));
			}
		}
		ImGui::EndMenu();
	}

	if (keys.empty()) {
		ImGui::TextDisabled("No prefabs");
	}

	ImGui::EndMenu();
}

template <typename MenuContext>
void DrawCreateArchetypeItems(MenuContext& context) {
	DrawCreateMenuItem(context, "Sprite", CreateDefaultSprite);
	DrawCreateMenuItem(context, "Sprite Stack", CreateDefaultSpriteStack);
	DrawCreateMenuItem(context, "Animation", CreateDefaultAnimation);
	DrawCreateMenuItem(context, "Particle Emitter", CreateDefaultParticleEmitter);
	DrawCreateMenuItem(context, "Light", CreateDefaultLight);
	DrawCreateMenuItem(context, "Render Target", CreateDefaultRenderTarget);

	// Cameras must remain at the scene root.
	DrawCreateMenuItem(context, "Camera", CreateDefaultCamera, false);

	DrawCreateMenuItem(context, "Custom Shader", CreateDefaultCustomShader);
	DrawCreateMenuItem(context, "Tilemap", [](EntityCreationContext creation) -> Entity {
		if (!creation.HasScene()) {
			return {};
		}
		return CreateTilemap(*creation.GetScene());
	});

	DrawEffectsCreateMenu(context);
	DrawShapesCreateMenu(context);
	DrawInteractiveCreateMenu(context);
	DrawUICreateMenu(context);
}

void DrawCreateEntityMenu(
	EditorContext& ctx, Scene& scene, Entity parent, Entity& selected_entity
) {
	CreateMenuContext context{
		.ctx{ ctx },
		.scene{ scene },
		.parent{ parent },
		.selected_entity{ selected_entity },
	};

	if (parent && (IsTilemap(parent) || GetChildAcceptanceLockReason(parent).has_value())) {
		return;
	}

	// The only direct children permitted below a Group Stack are Groups.
	if (parent && parent.Has<GroupStackData>()) {
		if (ImGui::MenuItem("Create Child Group")) {
			FinalizeCreatedEntity(context, CreateDefaultGroupEntity(scene), "Group");
		}
		ImGui::Separator();
		return;
	}

	bool creating_child{ context.IsCreatingChild() };
	const char* create_entity_label{ creating_child ? "Create Empty Child"
													: "Create Empty Entity" };
	const char* create_submenu_label{ creating_child ? "Create Child" : "Create Entity" };

	if (ImGui::MenuItem(create_entity_label)) {
		FinalizeCreatedEntity(context, scene.CreateEntity(), "Entity");
	}

	if (ImGui::BeginMenu(create_submenu_label)) {
		DrawCreateArchetypeItems(context);
		DrawPrefabCreateMenu(context);
		ImGui::EndMenu();
	}

	ImGui::Separator();
	if (ImGui::MenuItem(creating_child ? "Create Child Group" : "Create Group")) {
		FinalizeCreatedEntity(context, CreateDefaultGroupEntity(scene), "Group");
	}

	if (parent && parent.Has<Group>()) {
		ImGui::Separator();
	}

	if (!parent && ImGui::MenuItem("Create Group Stack")) {
		FinalizeCreatedEntity(context, CreateDefaultGroupStack(scene), "Group Stack");
	}
}

void DrawSceneHierarchyContents(
	EditorContext& ctx, Scene& scene, const std::optional<path>& project_root,
	Entity& selected_entity, std::optional<PrefabKey>& selected_prefab,
	std::array<char, 256>& filter, std::optional<SceneEntitySelection>& renaming_entity,
	std::string& entity_rename_text, std::string& entity_rename_error, bool& focus_entity_rename,
	bool show_managed_ui_parts
) {
	Entity entity_to_rename;
	std::string entity_rename_name;
	Entity entity_to_duplicate;
	Entity entity_to_delete;
	PendingHierarchyDrop pending_drop;

	RenameEditStateRef entity_rename_state{ focus_entity_rename, entity_rename_text,
											entity_rename_error };

	if (renaming_entity.has_value() && (renaming_entity->scene_key != scene.GetTag() ||
										renaming_entity->runtime != scene.IsRuntime() ||
										!renaming_entity->entity_uuid.has_value() ||
										!scene.GetEntity(renaming_entity->entity_uuid.value()))) {
		renaming_entity.reset();
		CancelRename(entity_rename_state);
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint(
		"##HierarchyFilter", "Filter: player, -enemy, *hidden, *shown", filter.data(), filter.size()
	);

	hierarchy::DrawFilterTooltip();

	ImGui::Separator();

	auto filter_text{ std::string_view{ filter.data() } };

	bool entity_left_clicked_this_frame{ false };

	auto draw_entity = [&](auto&& self, Entity entity, std::size_t recursion_depth) -> void {
		if (!entity || !scene.Entities().Contains(entity)) {
			return;
		}

		PTGN_ASSERT(
			recursion_depth <= kMaxParentDepth,
			"Maximum parent depth exceeded while sorting entities by depth. "
			"This likely indicates a cycle in the entity hierarchy"
		);

		if (!EntityOrDescendantMatchesFilter(entity, filter_text, show_managed_ui_parts)) {
			return;
		}

		bool selected{ entity == selected_entity };
		const PrefabInstance* linked_instance{ GetPrefabInstance(entity) };
		Entity instance_root{ linked_instance ? GetPrefabInstanceRoot(entity) : Entity{} };
		bool linked_root{ linked_instance && instance_root == entity };
		bool linked_child{ linked_instance && !linked_root };
		std::vector<Entity> children;

		if (HasChildren(entity)) {
			children = GetChildren(entity);
		}

		children.erase(
			std::remove_if(
				children.begin(), children.end(),
				[&](Entity child) {
					return !child || !scene.Entities().Contains(child) ||
						   !EntityOrDescendantMatchesFilter(
							   child, filter_text, show_managed_ui_parts
						   );
				}
			),
			children.end()
		);
		SortByLocalDepth(children);

		bool has_visible_children{ !children.empty() };

		UUID entity_uuid{ entity.Get<UUID>() };
		ImGui::PushID(entity_uuid);

		bool renaming{ renaming_entity.has_value() &&
					   renaming_entity->scene_key == scene.GetTag() &&
					   renaming_entity->runtime == scene.IsRuntime() &&
					   renaming_entity->entity_uuid == entity_uuid };

		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_OpenOnArrow |
								  ImGuiTreeNodeFlags_OpenOnDoubleClick |
								  ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_DefaultOpen };

		if (selected) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		if (!has_visible_children) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		} else if (!filter_text.empty()) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}

		Entity dragged{ GetDraggedEntity(scene) };
		bool hierarchy_drag_active{ static_cast<bool>(dragged) };
		bool can_drop_on_entity{ hierarchy_drag_active && CanReparent(dragged, entity) };
		bool invalid_entity_drop_target{ !renaming && hierarchy_drag_active &&
										 !can_drop_on_entity };

		if (invalid_entity_drop_target) {
			constexpr ImVec4 kTransparent{ 0.0f, 0.0f, 0.0f, 0.0f };

			ImGui::PushStyleColor(ImGuiCol_Header, kTransparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kTransparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, kTransparent);
		}

		// groups have independent, transient editor visibility. This never edits
		// the runtime stack history or the serialized initial group.
		if (entity.Has<Group>() && !scene.IsRuntime() && !ctx.editor.IsPlaying() &&
			!ctx.editor.IsDirectRuntime()) {
			GroupStack stack{ GetGroupStack(entity) };
			if (stack) {
				bool visible{ stack.IsGroupPreviewVisible(entity) };
				if (DrawEditorIconButton(
						"##GroupEntityVisible", visible ? EditorIcon::Visible : EditorIcon::Hidden,
						EditorIconButtonOptions{
							.tooltip = visible ? "Hide group in editor viewport"
											   : "Show group in editor viewport",
							.compact = true,
							.muted	 = !visible,
						}
					)) {
					stack.SetGroupPreviewVisible(entity, !visible);
				}
				ImGui::SameLine(0.0f, 2.0f);
			}
		}

		auto label{ entity.Get<Tag>() };
		// Place the link icon in the preceding indentation without shifting the entity name.
		std::string_view link_prefix{ "    " };
		bool show_link_icon{ linked_root && !renaming };
		float link_outdent{ show_link_icon ? ImGui::CalcTextSize(link_prefix.data()).x : 0.0f };
		if (show_link_icon) {
			ImGui::Unindent(link_outdent);
		}
		std::string display_label{ label.value };
		if (linked_root) {
			display_label += " [" + GetPrefabDisplayName(linked_instance->prefab) + "]";
		}
		if (show_link_icon) {
			display_label.insert(0, link_prefix);
		}
		auto node{ DrawRenamableTreeNode(
			renaming, entity_rename_state, "##Entity", display_label, flags,
			[&](std::string_view candidate) {
				std::string name{ TrimWhitespace(candidate) };
				return name.empty() ? std::string{ "Entity name cannot be empty." } : std::string{};
			},
			[&](std::string_view candidate) {
				std::string name{ TrimWhitespace(candidate) };
				if (name != entity.Get<Tag>().value) {
					entity_to_rename   = entity;
					entity_rename_name = std::move(name);
				}
			},
			RenamableTreeNodeOptions{
				.rename_width	 = -FLT_MIN,
				.rename_input_id = "##RenameEntity",
			}
		) };
		if (show_link_icon) {
			ImGui::Indent(link_outdent);
		}

		if (invalid_entity_drop_target) {
			ImGui::PopStyleColor(3);
		}

		if (show_link_icon) {
			float extent{ ImGui::GetFontSize() };
			ImVec2 row_min{ ImGui::GetItemRectMin() };
			ImVec2 row_max{ ImGui::GetItemRectMax() };
			float label_x{ row_min.x + ImGui::GetTreeNodeToLabelSpacing() + link_outdent };
			ImVec2 link_pos{
				label_x - extent - 2.0f,
				row_min.y + (row_max.y - row_min.y - extent) * 0.5f,
			};
			bool related_prefab_selected{ selected_prefab.has_value() &&
										  ctx.local.selection.mode ==
											  EditorSelectionMode::Prefabs &&
										  *selected_prefab == linked_instance->prefab };
			DrawEditorIcon(
				ImGui::GetWindowDrawList(), EditorIcon::Link, link_pos, extent,
				related_prefab_selected ? kPrefabLinkHighlightColor
										: ImGui::GetColorU32(ImGuiCol_TextDisabled)
			);
			if (ImGui::IsItemHovered() &&
				ImGui::IsMouseHoveringRect(
					link_pos, ImVec2{ link_pos.x + extent, link_pos.y + extent }
				)) {
				ImGui::SetTooltip("Linked prefab instance");
			}
		}

		if (renaming) {
			if (node.left_clicked) {
				entity_left_clicked_this_frame = true;
			}

			if (node.rename_result != RenameResult::None) {
				renaming_entity.reset();
				CancelRename(entity_rename_state);
			}

			ImGui::PopID();
			return;
		}

		bool open{ node.open };

		if (node.left_clicked) {
			RequestPrimaryInspector(ctx, EditorSelectionMode::SceneHierarchy);
			selected_entity				   = entity;
			entity_left_clicked_this_frame = true;

			if (entity.Has<Group>() && !scene.IsRuntime() && !ctx.editor.IsPlaying() &&
				!ctx.editor.IsDirectRuntime() && ctx.editor.GetSettings().preview_selected_group) {
				GroupStack stack{ GetGroupStack(entity) };

				if (stack) {
					stack.ShowOnlyGroupPreview(entity);
				}
			}
		}

		if (node.right_clicked) {
			RequestPrimaryInspector(ctx, EditorSelectionMode::SceneHierarchy);
			selected_entity = entity;
		}

		auto hierarchy_restriction_reason{ GetHierarchyRestrictionReason(entity) };
		auto duplication_lock_reason{ GetDuplicationLockReason(entity) };
		auto deletion_lock_reason{ GetDeletionLockReason(entity) };
		auto child_acceptance_reason{ GetChildAcceptanceLockReason(entity) };
		bool protected_primary{ IsPrimarySceneRenderTarget(entity) ||
								IsPrimarySceneCamera(entity) || IsFixedSceneCamera(entity) };

		if (!linked_child && ImGui::BeginDragDropSource()) {
			auto uuid{ entity.Get<UUID>() };
			ImGui::SetDragDropPayload(kEntityDragDropPayload, &uuid, sizeof(uuid));
			ImGui::Text("Move %s", label.value.c_str());
			ImGui::EndDragDropSource();
		}

		if (can_drop_on_entity && ImGui::BeginDragDropTarget()) {
			if (Entity dropped{ AcceptDraggedEntity(scene) };
				dropped && CanReparent(dropped, entity)) {
				pending_drop = PendingHierarchyDrop{
					.entity{ dropped },
					.parent{ entity },
				};
			}

			ImGui::EndDragDropTarget();
		}

		DrawRenamableContextMenu(
			node, "EntityContext", label.value,
			[&](std::string_view current_name) {
				renaming_entity = SceneEntitySelection{
					.scene_key	 = scene.GetTag(),
					.runtime	 = scene.IsRuntime(),
					.entity_uuid = entity_uuid,
				};
				BeginRename(entity_rename_state, current_name);
			},
			[&]() {
				ImGui::BeginDisabled(linked_instance != nullptr);
				if (IsTilemap(entity)) {
					if (ImGui::MenuItem("Create Generator")) {
						PaintGenerator created{ CreatePaintGenerator(scene) };

						if (created) {
							if (created.SetTargetTilemap(Tilemap{ entity })) {
								scene.Refresh();
								selected_entity =
									ctx.commands.RecordCreatedEntity(created, ctx.local.selection);
							} else {
								created.Destroy();
								scene.Refresh();
							}
						}
					}
				} else if (!child_acceptance_reason.has_value()) {
					DrawCreateEntityMenu(ctx, scene, entity, selected_entity);
				}
				ImGui::EndDisabled();

				if (!protected_primary) {
					if (linked_instance) {
						ImGui::Separator();
						ImGui::BeginDisabled(!instance_root);
						if (ImGui::MenuItem("Convert to Entity")) {
							ctx.commands.ConvertPrefabInstanceToEntity(instance_root);
						}
						ImGui::EndDisabled();
						if (ImGui::MenuItem("Go to Prefab")) {
							// Keep the hierarchy's local selection in sync so the end of this frame
							// cannot restore an older prefab over the navigation target.
							PrefabKey prefab_key{ linked_instance->prefab };
							SerializedEntityPath prefab_path{ linked_instance->entity_path };
							selected_prefab = prefab_key;
							ctx.editor.GetSceneHierarchyPanel().SetSelectedPrefab(
								prefab_key, std::move(prefab_path), false
							);
							ImGui::SetWindowFocus("Prefabs###PrefabsWindow");
						}
					} else if (!entity.Has<Group>() && !entity.Has<GroupStackData>()) {
						ImGui::Separator();
						ImGui::BeginDisabled(!project_root.has_value());
						if (ImGui::MenuItem("Create Prefab")) {
							PrefabKey created_key{
								ctx.commands.CreatePrefabAsset(CreatePrefabFromEntity(
									scene.ctx().asset, project_root.value(), entity
								))
							};
							if (!created_key.value.empty()) {
								selected_prefab = created_key;
								ImGui::SetWindowFocus("Prefabs###PrefabsWindow");
							}
						}
						ImGui::EndDisabled();
					}

					if (linked_instance ||
						(!entity.Has<Group>() && !entity.Has<GroupStackData>())) {
						ImGui::Separator();
					}
				}
			},
			[&]() {
				if (!protected_primary) {
					ImGui::BeginDisabled(linked_child || duplication_lock_reason.has_value());
					if (ImGui::MenuItem("Duplicate")) {
						entity_to_duplicate = entity;
					}
					ImGui::EndDisabled();

					ImGui::Separator();
					ImGui::BeginDisabled(linked_child || deletion_lock_reason.has_value());
					if (ImGui::MenuItem("Delete")) {
						entity_to_delete = entity;
					}
					ImGui::EndDisabled();
				}

				DrawRestrictionReasons(
					{
						child_acceptance_reason,
						hierarchy_restriction_reason,
						duplication_lock_reason,
						deletion_lock_reason,
					}
				);
			},
			RenamableContextMenuOptions{
				.show_rename	  = !protected_primary && !linked_instance,
				.rename_enabled	  = true,
				.rename_placement = RenameMenuPlacement::Middle,
			}
		);

		if (has_visible_children && open) {
			for (Entity child : children) {
				self(self, child, recursion_depth + 1);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	};

	// All scene entities are displayed directly at the hierarchy root. Tilemaps
	// are ordinary entities, not the contents of an organizational tile layer.
	std::vector<Entity> roots;
	for (Entity entity : scene.Entities()) {
		if (IsRuntimeWorldContent(entity)) {
			continue;
		}
		if (entity && !HasParent(entity)) {
			roots.push_back(entity);
		}
	}
	SortByLocalDepth(roots);
	for (Entity entity : roots) {
		draw_entity(draw_entity, entity, 0);
	}

	// A distinct target below the final tree item lets dragged children return to the scene root.
	// It must be an ImGui item to accept a drag; window whitespace alone cannot do that.
	float blank_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	float blank_height{ std::max(32.0f, ImGui::GetContentRegionAvail().y) };
	ImGui::InvisibleButton("##HierarchyRootDropTarget", ImVec2{ blank_width, blank_height });
	if (ImGui::BeginDragDropTarget()) {
		if (Entity dropped{ AcceptDraggedEntity(scene) }; dropped && CanReparent(dropped, {})) {
			pending_drop = PendingHierarchyDrop{ .entity = dropped, .parent = {} };
		}
		ImGui::EndDragDropTarget();
	}

	// The blank hierarchy area can create entities, including Tilemaps.
	if (ImGui::BeginPopupContextItem(
			"SceneHierarchyPanelContextMenu", ImGuiPopupFlags_MouseButtonRight
		)) {
		selected_entity = {};
		DrawCreateEntityMenu(ctx, scene, {}, selected_entity);
		ImGui::EndPopup();
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!entity_left_clicked_this_frame /* && !ImGui::IsAnyItemHovered() */) {
		selected_entity = {};
	}

	bool delete_pressed{ ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
						 ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
						 !ImGui::GetIO().WantTextInput };

	if (!entity_to_delete && selected_entity && delete_pressed) {
		entity_to_delete = selected_entity;
	}

	if (pending_drop) {
		ApplyHierarchyDrop(ctx, pending_drop);
	}

	if (entity_to_rename) {
		ctx.commands.RenameEntity(entity_to_rename, entity_rename_name);
	}

	if (entity_to_duplicate && !GetDuplicationLockReason(entity_to_duplicate).has_value()) {
		selected_entity = ctx.commands.DuplicateEntity(entity_to_duplicate);
	}

	if (entity_to_delete && !GetDeletionLockReason(entity_to_delete).has_value()) {
		auto& paint{ ctx.editor.GetPaintEditor() };
		if (paint.IsActiveBrushGenerator(entity_to_delete)) {
			paint.CancelActiveBrushGenerator(ctx, scene, true);
			selected_entity = ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity();
		} else {
			if (selected_entity == entity_to_delete) {
				selected_entity = {};
			}
			ctx.commands.DeleteEntity(entity_to_delete);
		}
	}
}

} // namespace

bool CanReparentHierarchyEntity(Entity entity, Entity parent) {
	return CanReparent(entity, parent);
}

bool CanAssignEntityToGroup(Entity entity, Entity layer) {
	return entity && layer && layer.Has<Group>() && !entity.Has<GroupStackData>() &&
		   CanReparent(entity, layer);
}

bool SceneHierarchyPanel::DrawSceneHierarchy(EditorContext& ctx) {
	bool visible{ ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow") };

	if (visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		SetActiveTab(SceneHierarchyTab::SceneHierarchy);
	}

	const auto& scene_list{ ctx.editor.GetSceneListPanel() };

	auto* selected_scene{ scene_list.GetSelectedScene() };

	if (selected_scene) {
		Entity selected_entity{ GetSelectedEntity() };
		auto selected_prefab{ GetSelectedPrefab() };

		DrawSceneHierarchyContents(
			ctx, *selected_scene, ctx.editor.GetProjectRoot(), selected_entity, selected_prefab,
			filter_, renaming_entity_, entity_rename_text_, entity_rename_error_,
			focus_entity_rename_, ctx.editor.GetSettings().show_managed_ui_parts
		);

		if (selected_entity && !selected_scene->Entities().Contains(selected_entity)) {
			selected_entity = {};
		}

		if (selected_entity != GetSelectedEntity()) {
			SetSelectedEntity(selected_entity);
		}

		if (selected_prefab != GetSelectedPrefab()) {
			SetSelectedPrefab(std::move(selected_prefab));
		}
	} else {
		renaming_entity_.reset();
		entity_rename_text_.clear();
		entity_rename_error_.clear();
		focus_entity_rename_ = false;
	}

	ImGui::End();
	return visible;
}

bool SceneHierarchyPanel::DrawPrefabs(EditorContext& ctx) {
	bool visible{ ImGui::Begin("Prefabs###PrefabsWindow") };
	if (visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		SetActiveTab(SceneHierarchyTab::Prefabs);
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	auto& paint{ ctx.editor.GetPaintEditor() };
	const auto& scene_list{ ctx.editor.GetSceneListPanel() };
	auto* selected_scene{ scene_list.GetSelectedScene() };
	auto project_root{ ctx.editor.GetProjectRoot() };

	auto create_blank_prefab = [&](std::string_view group) {
		if (!project_root.has_value()) {
			return;
		}
		PrefabKey key{
			ctx.commands.CreatePrefabAsset(CreateBlankPrefab(assets, project_root.value()))
		};
		if (!key.value.empty()) {
			if (group != "Ungrouped") {
				std::size_t after_creation{ ctx.undo.Cursor() };
				paint.MovePrefabToGroup(ctx, key, std::string{ group });
				if (ctx.undo.Cursor() == after_creation + 1) {
					(void)ctx.undo.MergeLastAppliedCommands("Create Prefab");
				}
			}
			SetSelectedPrefab(key);
		}
	};

	auto draw_prefab_creation_items = [&](std::string_view group) {
		ImGui::BeginDisabled(!project_root.has_value());
		if (ImGui::MenuItem("Create Empty Prefab")) {
			create_blank_prefab(group);
		}
		ImGui::BeginDisabled(!selected_scene);
		if (ImGui::BeginMenu("Create Prefab")) {
			if (project_root.has_value() && selected_scene) {
				PrefabArchetypeMenuContext context{
					.ctx		  = ctx,
					.scene		  = *selected_scene,
					.assets		  = assets,
					.project_root = project_root.value(),
					.group		  = group,
				};
				DrawCreateArchetypeItems(context);
			}
			ImGui::EndMenu();
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();
	};

	auto draw_prefab_context_creation = [&](std::string_view group) {
		if (ImGui::MenuItem("Create Prefab Group")) {
			auto groups{ paint.GetPrefabGroups(ctx) };
			paint.CreatePrefabGroup(ctx, NextDefaultGroupName(groups));
		}
		ImGui::Separator();
		draw_prefab_creation_items(group);
	};

	static std::string prefab_filter;
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##PrefabGroupFilter", "Filter: tree, environment, -enemy", &prefab_filter
	);
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Prefab filter syntax:");
		ImGui::Separator();
		ImGui::TextUnformatted("tree");
		ImGui::SameLine();
		ImGui::TextDisabled("Prefab name or key contains \"tree\"");
		ImGui::TextUnformatted("environment");
		ImGui::SameLine();
		ImGui::TextDisabled("Group contains \"environment\"");
		ImGui::TextUnformatted("-enemy");
		ImGui::SameLine();
		ImGui::TextDisabled("Name, key and group do not contain \"enemy\"");
		ImGui::Spacing();
		ImGui::TextDisabled("Separate filters with commas.");
		ImGui::TextDisabled("Positive filters use OR; all exclusions apply.");
		ImGui::TextDisabled("Matching ignores letter case.");
		ImGui::EndTooltip();
	}
	ImGui::Separator();

	enum class PrefabEntityOperationType {
		AddChild,
		Rename,
		Duplicate,
		Delete
	};

	struct PendingPrefabEntityOperation {
		PrefabEntityOperationType type{};
		PrefabKey key{};
		SerializedEntityPath path{};
		std::optional<SerializedEntity> child{};
		std::string name{};
	};

	std::optional<std::pair<PrefabKey, PrefabDeletionMode>> prefab_to_delete{};
	bool open_prefab_delete_options{ false };
	std::optional<PrefabKey> prefab_to_duplicate{};
	std::optional<std::pair<PrefabKey, PrefabKey>> prefab_to_rename{};
	std::optional<std::pair<PrefabKey, std::string>> prefab_group_move{};
	std::optional<PendingPrefabEntityOperation> entity_operation{};
	bool prefab_left_clicked_this_frame{};

	static std::optional<std::pair<PrefabKey, SerializedEntityPath>> renaming_prefab_child;
	static std::string prefab_child_rename_text;
	static std::string prefab_child_rename_error;
	static bool focus_prefab_child_rename{};

	if (visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		if (const auto selected_prefab{ GetSelectedPrefab() }) {
			const auto& selected_path{ GetSelectedPrefabEntityPath() };
			if (selected_path.empty()) {
				open_prefab_delete_options = true;
			} else {
				entity_operation = PendingPrefabEntityOperation{
					.type = PrefabEntityOperationType::Delete,
					.key  = *selected_prefab,
					.path = selected_path,
				};
			}
		}
	}

	static std::optional<std::string> group_to_rename;
	static RenameModalState group_rename_modal;

	auto all_keys{ assets.GetPrefabKeys() };
	std::ranges::sort(all_keys, {}, [](const PrefabKey& key) { return key.value; });
	auto groups{ paint.GetPrefabGroups(ctx) };

	std::optional<PrefabKey> selected_instance_prefab;
	if (ctx.local.selection.mode == EditorSelectionMode::SceneHierarchy) {
		Entity selected_scene_entity{ GetSelectedEntity() };
		if (const PrefabInstance* link{ GetPrefabInstance(selected_scene_entity) }) {
			selected_instance_prefab = link->prefab;
		}
	}

	auto draw_prefab = [&](const PrefabKey& key) {
		ImGui::PushID(key.value.c_str());
		if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(key)) {
			if (auto catalog{ assets.GetCatalogAsset(key, AssetKind::Prefab) };
				catalog.has_value()) {
				assets.Load(key, catalog->source_path);
			}
		}
		if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(key)) {
			ImGui::TextDisabled("%s (unavailable)", GetPrefabDisplayName(key).c_str());
			ImGui::PopID();
			return;
		}
		auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(key) };
		auto& prefab{ prefab_asset.get() };

		auto draw_prefab_entity = [&](auto&& self, SerializedEntity& entity,
									  SerializedEntityPath entity_path, bool root) -> void {
			if (!root && std::ranges::any_of(entity.tags, [](const std::string& tag) {
					return tag.ends_with("InteractiveTag");
				})) {
				return;
			}
			std::string entity_id{ "root" };
			for (std::size_t index : entity_path) {
				entity_id += "/" + std::to_string(index);
			}
			ImGui::PushID(entity_id.c_str());

			bool selected{ GetSelectedPrefab() == key &&
						   GetSelectedPrefabEntityPath() == entity_path };
			bool has_children{ !entity.children.empty() };
			if (root && renaming_prefab_ == key) {
				RenameEditStateRef rename_state{ focus_prefab_rename_, prefab_rename_text_,
												 prefab_rename_error_ };
				RenameResult result{ DrawRenameInput(
					rename_state, "##RenamePrefab", -FLT_MIN,
					[&](std::string_view candidate) -> std::string {
						std::string name{ TrimWhitespace(candidate) };
						if (name.empty()) {
							return "Prefab name cannot be empty.";
						}
						if (!project_root.has_value()) {
							return "No project path is available.";
						}
						PrefabKey new_key{ MakePrefabKey(name) };
						return IsPrefabKeyAvailable(assets, *project_root, key, new_key)
								 ? std::string{}
								 : std::string{ "A prefab with that key already exists." };
					},
					[&](std::string_view candidate) {
						PrefabKey new_key{ MakePrefabKey(TrimWhitespace(candidate)) };
						if (new_key != key) {
							prefab_to_rename = std::pair{ key, std::move(new_key) };
						}
					}
				) };
				if (result != RenameResult::None) {
					renaming_prefab_.reset();
					CancelRename(rename_state);
				}
				ImGui::PopID();
				return;
			}

			if (!root && renaming_prefab_child == std::optional{ std::pair{ key, entity_path } }) {
				RenameEditStateRef rename_state{ focus_prefab_child_rename,
												 prefab_child_rename_text,
												 prefab_child_rename_error };
				RenameResult result{ DrawRenameInput(
					rename_state, "##RenamePrefabChild", -FLT_MIN,
					[](std::string_view candidate) -> std::string {
						return TrimWhitespace(candidate).empty()
								 ? std::string{ "Entity name cannot be empty." }
								 : std::string{};
					},
					[&](std::string_view candidate) {
						std::string name{ TrimWhitespace(candidate) };
						if (name != entity.tag) {
							entity_operation = PendingPrefabEntityOperation{
								.type = PrefabEntityOperationType::Rename,
								.key  = key,
								.path = entity_path,
								.name = std::move(name),
							};
						}
					}
				) };
				if (result != RenameResult::None) {
					renaming_prefab_child.reset();
					CancelRename(rename_state);
				}
				ImGui::PopID();
				return;
			}

			ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_OpenOnArrow |
									  ImGuiTreeNodeFlags_OpenOnDoubleClick |
									  ImGuiTreeNodeFlags_SpanAvailWidth |
									  ImGuiTreeNodeFlags_DefaultOpen };
			if (selected) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}
			if (!has_children) {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			std::string label{ root ? GetPrefabDisplayName(key) : entity.tag };
			bool related_instance_selected{ root && selected_instance_prefab.has_value() &&
											*selected_instance_prefab == key };
			bool reveal_selected{ force_open_prefab_ == key &&
								  entity_path.size() <= force_open_prefab_entity_path_.size() &&
								  std::equal(
									  entity_path.begin(), entity_path.end(),
									  force_open_prefab_entity_path_.begin()
								  ) };
			if (reveal_selected && has_children) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
			}
			if (related_instance_selected) {
				ImGui::PushStyleColor(
					ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kPrefabLinkHighlightColor)
				);
			}
			bool open{ ImGui::TreeNodeEx("##PrefabEntity", flags, "%s", label.c_str()) };
			if (related_instance_selected) {
				ImGui::PopStyleColor();
			}
			if (reveal_selected && entity_path == force_open_prefab_entity_path_) {
				ImGui::SetScrollHereY(0.5f);
				force_open_prefab_.reset();
				force_open_prefab_entity_path_.clear();
			}
			bool prefab_hovered{ ImGui::IsItemHovered() };
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				SetSelectedPrefab(key, entity_path);
				prefab_left_clicked_this_frame = true;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				SetSelectedPrefab(key, entity_path, false);
				ImGui::OpenPopup("PrefabEntityContextMenu");
			}
			if (root && selected_scene && prefab_hovered &&
				ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				ctx.commands.CreatePrefabInstance(*selected_scene, key);
				ImGui::SetWindowFocus("Scene Hierarchy###SceneHierarchyWindow");
			}
			if (root && ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload(
					kPrefabAssetDragDropPayload, key.value.c_str(), key.value.size() + 1
				);
				ImGui::TextUnformatted(label.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginPopup("PrefabEntityContextMenu")) {
				if (ImGui::MenuItem("Create Empty Child")) {
					entity_operation = PendingPrefabEntityOperation{
						.type = PrefabEntityOperationType::AddChild, .key = key, .path = entity_path
					};
				}
				ImGui::BeginDisabled(!selected_scene);
				if (ImGui::BeginMenu("Create Child")) {
					std::optional<SerializedEntity> child;
					PrefabChildMenuContext context{ ctx, *selected_scene, key, child };
					DrawCreateArchetypeItems(context);
					if (child.has_value()) {
						entity_operation = PendingPrefabEntityOperation{
							.type  = PrefabEntityOperationType::AddChild,
							.key   = key,
							.path  = entity_path,
							.child = std::move(child),
						};
					}
					ImGui::EndMenu();
				}
				ImGui::EndDisabled();
				if (root) {
					ImGui::Separator();
					ImGui::BeginDisabled(!selected_scene);
					if (ImGui::MenuItem("Create Instance")) {
						ctx.commands.CreatePrefabInstance(*selected_scene, key);
						ImGui::SetWindowFocus("Scene Hierarchy###SceneHierarchyWindow");
					}
					ImGui::EndDisabled();
					ImGui::Separator();
					ImGui::BeginDisabled(!project_root.has_value());
					if (ImGui::MenuItem("Rename")) {
						SetSelectedPrefab(key, {}, false);
						renaming_prefab_	= key;
						prefab_rename_text_ = label;
						prefab_rename_error_.clear();
						focus_prefab_rename_ = true;
					}
					if (ImGui::MenuItem("Duplicate")) {
						prefab_to_duplicate = key;
					}
					ImGui::EndDisabled();
					ImGui::Separator();
					if (ImGui::MenuItem("Delete and Unlink Instances")) {
						prefab_to_delete = std::pair{ key, PrefabDeletionMode::UnlinkInstances };
					}
					if (ImGui::MenuItem("Delete with Instances")) {
						prefab_to_delete = std::pair{ key, PrefabDeletionMode::DeleteInstances };
					}
				} else {
					ImGui::Separator();
					if (ImGui::MenuItem("Rename")) {
						renaming_prefab_child	 = std::pair{ key, entity_path };
						prefab_child_rename_text = entity.tag;
						prefab_child_rename_error.clear();
						focus_prefab_child_rename = true;
					}
					if (ImGui::MenuItem("Duplicate")) {
						entity_operation =
							PendingPrefabEntityOperation{ .type =
															  PrefabEntityOperationType::Duplicate,
														  .key	= key,
														  .path = entity_path };
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Delete")) {
						entity_operation =
							PendingPrefabEntityOperation{ .type = PrefabEntityOperationType::Delete,
														  .key	= key,
														  .path = entity_path };
					}
				}
				ImGui::EndPopup();
			}

			if (has_children && open) {
				for (std::size_t i{}; i < entity.children.size(); ++i) {
					auto child_path{ entity_path };
					child_path.emplace_back(i);
					self(self, entity.children[i], std::move(child_path), false);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		};

		draw_prefab_entity(draw_prefab_entity, prefab.root, {}, true);
		ImGui::PopID();
	};

	auto matches_prefab = [&](const PrefabKey& key, std::string_view group) {
		std::string display_name{ GetPrefabDisplayName(key) };
		return MatchesTextFilter(prefab_filter, { group, display_name, key.value });
	};

	for (const std::string& group : groups) {
		bool group_matches{ MatchesTextFilter(prefab_filter, { group }) };
		bool has_match{ std::ranges::any_of(all_keys, [&](const PrefabKey& key) {
			return paint.GetPrefabGroup(ctx, key) == group &&
				   (matches_prefab(key, group) || force_open_prefab_ == key);
		}) };
		if (!has_match && !group_matches) {
			continue;
		}
		ImGui::PushID(group.c_str());
		if (force_open_prefab_.has_value() &&
			paint.GetPrefabGroup(ctx, *force_open_prefab_) == group) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}
		bool open{ ImGui::TreeNodeEx(
			"##PrefabGroup", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
			"%s", group.c_str()
		) };
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{
					ImGui::AcceptDragDropPayload(kPrefabAssetDragDropPayload) };
				payload && payload->Data && payload->DataSize > 1) {
				prefab_group_move = std::pair{
					PrefabKey{ std::string{ static_cast<const char*>(payload->Data) } },
					group,
				};
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("PrefabGroupContext")) {
			draw_prefab_creation_items(group);
			if (group != "Ungrouped") {
				ImGui::Separator();
				if (ImGui::MenuItem("Rename")) {
					group_to_rename = group;
					group_rename_modal.Begin(group);
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete")) {
					paint.DeletePrefabGroup(ctx, group);
				}
			}
			ImGui::EndPopup();
		}
		if (open) {
			for (const auto& key : all_keys) {
				if (paint.GetPrefabGroup(ctx, key) != group) {
					continue;
				}
				if (!matches_prefab(key, group) && force_open_prefab_ != key) {
					continue;
				}
				draw_prefab(key);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (prefab_group_move.has_value()) {
		const auto& [key, group]{ *prefab_group_move };
		if (paint.GetPrefabGroup(ctx, key) != group) {
			paint.MovePrefabToGroup(ctx, key, group);
		}
	}

	if (group_to_rename.has_value()) {
		RenameResult result{ DrawRenameModal(
			group_rename_modal, "RenamePrefabGroup", "##GroupName",
			[&](std::string_view candidate) -> std::string {
				std::string name{ TrimWhitespace(candidate) };
				if (name.empty() || name == "Ungrouped") {
					return "Enter a valid group name.";
				}
				if (name != *group_to_rename && std::ranges::contains(groups, name)) {
					return "A group with that name already exists.";
				}
				return {};
			},
			[&](std::string_view candidate) {
				paint.RenamePrefabGroup(
					ctx, *group_to_rename, std::string{ TrimWhitespace(candidate) }
				);
			},
			RenameModalOptions{ .title = "Rename Prefab Group" }
		) };
		if (result != RenameResult::None) {
			group_to_rename.reset();
		}
	}

	if (prefab_to_rename.has_value()) {
		const auto& [old_key, new_key]{ *prefab_to_rename };
		if (!ctx.commands.RenamePrefabAsset(old_key, new_key)) {
			renaming_prefab_	 = old_key;
			prefab_rename_text_	 = GetPrefabDisplayName(new_key);
			prefab_rename_error_ = "Failed to rename prefab.";
			focus_prefab_rename_ = true;
		} else {
			std::size_t after_rename{ ctx.undo.Cursor() };
			paint.OnPrefabRenamed(ctx, old_key, new_key);
			if (ctx.undo.Cursor() == after_rename + 1) {
				(void)ctx.undo.MergeLastAppliedCommands("Rename Prefab");
			}
		}
	}

	if (prefab_to_duplicate.has_value() && project_root.has_value()) {
		auto source{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(*prefab_to_duplicate) };
		PrefabKey requested_key{
			MakeUniquePrefabKey(assets, project_root.value(), source.get().root.tag)
		};
		PrefabKey duplicated_key{
			ctx.commands.DuplicatePrefabAsset(*prefab_to_duplicate, requested_key)
		};
		if (!duplicated_key.value.empty()) {
			std::size_t after_duplicate{ ctx.undo.Cursor() };
			paint.OnPrefabDuplicated(ctx, *prefab_to_duplicate, duplicated_key);
			if (ctx.undo.Cursor() == after_duplicate + 1) {
				(void)ctx.undo.MergeLastAppliedCommands("Duplicate Prefab");
			}
		}
	}

	if (entity_operation.has_value()) {
		switch (entity_operation->type) {
			case PrefabEntityOperationType::AddChild: {
				SerializedEntity child;
				if (entity_operation->child.has_value()) {
					child = std::move(*entity_operation->child);
				} else {
					child.tag = NextPrefabEntityName(assets, entity_operation->key, "Entity");
					AddDefaultPrefabSpatialComponents(child);
				}
				if (ctx.commands.AddPrefabChild(
						entity_operation->key, entity_operation->path, std::move(child)
					)) {
					force_open_prefab_			   = entity_operation->key;
					force_open_prefab_entity_path_ = entity_operation->path;
				}
				break;
			}
			case PrefabEntityOperationType::Rename:
				(void)RenameEmbeddedPrefabEntity(
					ctx, entity_operation->key, entity_operation->path, entity_operation->name
				);
				break;
			case PrefabEntityOperationType::Duplicate:
				ctx.commands.DuplicatePrefabEntity(entity_operation->key, entity_operation->path);
				break;
			case PrefabEntityOperationType::Delete:
				ctx.commands.DeletePrefabEntity(entity_operation->key, entity_operation->path);
				break;
		}
	}

	// The Delete key offers the same two choices as the context menu.
	if (open_prefab_delete_options) {
		ImGui::OpenPopup("PrefabDeleteOptions");
	}
	if (ImGui::BeginPopup("PrefabDeleteOptions")) {
		if (auto selected{ GetSelectedPrefab() }) {
			if (ImGui::MenuItem("Delete and Unlink Instances")) {
				prefab_to_delete = std::pair{ *selected, PrefabDeletionMode::UnlinkInstances };
			}
			if (ImGui::MenuItem("Delete with Instances")) {
				prefab_to_delete = std::pair{ *selected, PrefabDeletionMode::DeleteInstances };
			}
		}
		ImGui::EndPopup();
	}

	if (prefab_to_delete.has_value()) {
		const auto& [key, mode]{ *prefab_to_delete };
		if (ctx.commands.DeletePrefabAsset(key, mode)) {
			std::size_t after_deletion{ ctx.undo.Cursor() };
			paint.OnPrefabDeleted(ctx, key);
			if (ctx.undo.Cursor() == after_deletion + 1) {
				(void)ctx.undo.MergeLastAppliedCommands(
					mode == PrefabDeletionMode::UnlinkInstances ? "Delete and Unlink Instances"
																: "Delete with Instances"
				);
			}
			if (renaming_prefab_ == std::optional{ key }) {
				renaming_prefab_.reset();
				prefab_rename_text_.clear();
				prefab_rename_error_.clear();
			}
		}
	}

	// Item context menus take priority over the panel background menu. An item popup can
	// block hover detection when the right mouse button is released, so also check for
	// existing popups before opening the background menu.
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
		!ImGui::IsAnyItemHovered() &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
		ImGui::OpenPopup("PrefabsPanelContextMenu");
	}
	if (ImGui::BeginPopup("PrefabsPanelContextMenu")) {
		draw_prefab_context_creation("Ungrouped");
		ImGui::EndPopup();
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!prefab_left_clicked_this_frame && !ImGui::IsAnyItemHovered()) {
		SetSelectedPrefab(std::nullopt);
	}
	ImGui::End();
	return visible;
}

bool SceneHierarchyPanel::DrawTiles(EditorContext& ctx) {
	return ctx.editor.GetPaintEditor().DrawTilesPanel(ctx);
}

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	// The two libraries share a single dock window, never two docked tabs.
	// Selection can change while DrawSceneHierarchy is running, so render it first.
	DrawSceneHierarchy(ctx);
	Entity selected{ GetSelectedEntity() };
	if (selected && IsTilemap(selected)) {
		DrawTiles(ctx);
	} else {
		DrawPrefabs(ctx);
	}
}

void SceneHierarchyPanel::Bind(EditorContext& ctx) {
	context_ = &ctx;
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
	return context_ ? ResolveSelectedEntity(*context_) : Entity{};
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity, bool undoable) {
	if (!context_) {
		return;
	}

	if (entity) {
		RequestPrimaryInspector(*context_, EditorSelectionMode::SceneHierarchy);
	}

	EditorSelection selection{ context_->local.selection };
	if (entity) {
		auto& scene{ entity.GetScene() };
		selection.selected_scene_key	 = scene.GetTag();
		selection.selected_scene_runtime = scene.IsRuntime();
		selection.SetEntityUUID(scene.GetTag(), scene.IsRuntime(), entity.Get<UUID>());

	} else if (selection.HasSceneSelection()) {
		selection.SetEntityUUID(
			selection.selected_scene_key, selection.selected_scene_runtime, std::nullopt
		);
	}
	selection.mode = EditorSelectionMode::SceneHierarchy;

	if (undoable) {
		SetEditorSelection(*context_, std::move(selection), "Select Entity");
	} else {
		ApplyEditorSelection(*context_, std::move(selection));
	}
}

std::optional<PrefabKey> SceneHierarchyPanel::GetSelectedPrefab() const {
	return context_ ? context_->local.selection.selected_prefab : std::nullopt;
}

const SerializedEntityPath& SceneHierarchyPanel::GetSelectedPrefabEntityPath() const {
	static const SerializedEntityPath empty_path;

	return context_ ? context_->local.selection.selected_prefab_entity_path : empty_path;
}

void SceneHierarchyPanel::SetSelectedPrefab(std::optional<PrefabKey> prefab, bool undoable) {
	SetSelectedPrefab(std::move(prefab), {}, undoable);
}

void SceneHierarchyPanel::SetSelectedPrefab(
	std::optional<PrefabKey> prefab, SerializedEntityPath entity_path, bool undoable
) {
	if (!context_) {
		return;
	}

	if (!prefab.has_value()) {
		entity_path.clear();
	}

	EditorSelection selection{ context_->local.selection };
	if (prefab.has_value() && selection.mode == EditorSelectionMode::SceneHierarchy) {
		force_open_prefab_			   = *prefab;
		force_open_prefab_entity_path_ = entity_path;
	}
	selection.selected_prefab			  = std::move(prefab);
	selection.selected_prefab_entity_path = std::move(entity_path);
	selection.mode						  = EditorSelectionMode::Prefabs;

	if (selection.selected_prefab.has_value()) {
		RequestPrimaryInspector(*context_, EditorSelectionMode::Prefabs);
	}

	if (undoable) {
		SetEditorSelection(*context_, std::move(selection), "Select Prefab Entity");
	} else {
		ApplyEditorSelection(*context_, std::move(selection));
	}
}

SceneHierarchyTab SceneHierarchyPanel::GetActiveTab() const {
	return context_ ? context_->local.selection.mode : SceneHierarchyTab::SceneHierarchy;
}

void SceneHierarchyPanel::SetActiveTab(SceneHierarchyTab tab) {
	if (!context_) {
		return;
	}

	SetSceneHierarchyTab(*context_, tab);
}

} // namespace ptgn::editor
