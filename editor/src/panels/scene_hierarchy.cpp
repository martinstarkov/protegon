#include "panels/scene_hierarchy.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "panels/scene_list.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/edge_detection.h"
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
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/asset/prefab.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"

namespace ptgn::editor {

namespace {

constexpr const char* kEntityDragDropPayload{ "PTGN_HIERARCHY_ENTITY" };

using HierarchyCondition = bool (*)(Entity);

enum class ParentAssignmentPolicy {
	SameParentOnly,
	RootOnly
};

struct ParentAssignmentRule {
	HierarchyCondition condition;
	ParentAssignmentPolicy policy;
	std::string_view reason;
};

struct ChildAcceptanceRule {
	HierarchyCondition condition;
	std::string_view reason;
};

struct DeletionRule {
	HierarchyCondition condition;
	std::string_view reason;
};

bool IsManagedButtonVisual(Entity entity) {
	if (!ptgn::HasParent(entity) || !GetParent(entity).Has<impl::ButtonData>()) {
		return false;
	}

	return entity.HasAny<
		ButtonBackgroundVisuals, ButtonBorderVisuals, ButtonSpriteVisuals, ButtonTextVisuals>();
}

bool IsCameraEntity(Entity entity) {
	return entity.Has<impl::CameraData>();
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
	Entity entity;
	Entity parent;

	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(entity);
	}
};

bool EntityMatchesFilter(Entity entity, std::string_view filter_text) {
	if (filter_text.empty()) {
		return true;
	}

	auto name{ ToLower(entity.Get<Tag>().value) };
	auto filter{ std::string{ filter_text } };

	bool has_name_include{ false };
	bool matched_name_include{ false };

	bool require_hidden{ false };
	bool require_shown{ false };

	std::size_t start{ 0 };

	while (start <= filter.size()) {
		auto comma{ filter.find(',', start) };
		auto token{ comma == std::string::npos ? filter.substr(start)
											   : filter.substr(start, comma - start) };

		token = ToLower(TrimWhitespace(std::move(token)));

		if (!token.empty()) {
			if (token.front() == '*') {
				auto filter_name{ TrimWhitespace(token.substr(1)) };

				if (filter_name == "hidden") {
					require_hidden = true;
				} else if (filter_name == "shown") {
					require_shown = true;
				}
			} else {
				bool exclude{ token.front() == '-' };
				auto needle{ TrimWhitespace(exclude ? token.substr(1) : token) };

				if (!needle.empty()) {
					bool contains{ name.find(needle) != std::string::npos };

					if (exclude && contains) {
						return false;
					}

					if (!exclude) {
						has_name_include	  = true;
						matched_name_include |= contains;
					}
				}
			}
		}

		if (comma == std::string::npos) {
			break;
		}

		start = comma + 1;
	}

	// Conflicting property filters cannot both be satisfied.
	if (require_hidden && require_shown) {
		return false;
	}

	bool visible{ IsVisible(entity) };

	if (require_hidden && visible) {
		return false;
	}

	if (require_shown && !visible) {
		return false;
	}

	return !has_name_include || matched_name_include;
}

bool EntityOrDescendantMatchesFilter(
	Entity entity, std::string_view filter_text, std::size_t recursion_depth = 0
) {
	PTGN_ASSERT(
		recursion_depth <= kMaxParentDepth,
		"Maximum parent depth exceeded while filtering entities. "
		"This likely indicates a cycle in the entity hierarchy"
	);

	if (EntityMatchesFilter(entity, filter_text)) {
		return true;
	}

	if (!HasChildren(entity)) {
		return false;
	}

	for (Entity child : GetChildren(entity)) {
		if (EntityOrDescendantMatchesFilter(child, filter_text, recursion_depth + 1)) {
			return true;
		}
	}

	return false;
}

std::vector<Entity> GetSiblings(Scene& scene, Entity parent) {
	std::vector<Entity> siblings;

	if (parent) {
		if (HasChildren(parent)) {
			siblings = GetChildren(parent);
		}
	} else {
		for (Entity entity : scene.Entities()) {
			if (!HasParent(entity)) {
				siblings.emplace_back(entity);
			}
		}
	}

	SortByLocalDepth(siblings);
	return siblings;
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

void ApplyHierarchyDrop(const PendingHierarchyDrop& drop) {
	if (!drop || !CanReparent(drop.entity, drop.parent)) {
		return;
	}

	Entity previous_parent{ ptgn::HasParent(drop.entity) ? GetParent(drop.entity) : Entity{} };

	if (previous_parent == drop.parent) {
		return;
	}

	std::optional<Transform> world_transform;

	if (drop.entity.Has<Transform>()) {
		world_transform = GetWorldTransform(drop.entity);
	}

	if (drop.parent) {
		SetParent(drop.entity, drop.parent);
	} else if (ptgn::HasParent(drop.entity)) {
		RemoveParent(drop.entity);
	}

	if (world_transform.has_value()) {
		SetWorldTransform(drop.entity, world_transform.value());
	}
}

void DrawRestrictionReasons(std::initializer_list<std::optional<std::string_view>> reasons) {
	std::vector<std::string_view> drawn_reasons;
	bool drew_separator{ false };

	for (const auto& reason : reasons) {
		if (!reason.has_value() || reason->empty()) {
			continue;
		}

		if (std::ranges::find(drawn_reasons, reason.value()) != drawn_reasons.end()) {
			continue;
		}

		if (!drew_separator) {
			ImGui::Separator();
			drew_separator = true;
		}

		ImGui::TextDisabled("%.*s", static_cast<int>(reason->size()), reason->data());

		drawn_reasons.emplace_back(reason.value());
	}
}

constexpr V2_float kDefaultShapeSize{ 100, 100 };
constexpr float kDefaultShapeRadius{ 50.0f };
constexpr V2_float kDefaultInteractiveSpriteSize{ 100, 100 };

constexpr V2_float kDefaultUIButtonSize{ 180, 50 };
constexpr V2_float kDefaultUISquareButtonSize{ 64, 64 };
constexpr V2_float kDefaultUISpriteSize{ 40, 40 };
constexpr V2_float kDefaultUIPanelSize{ 320, 180 };
constexpr V2_float kDefaultUIDialogueSize{ 640, 160 };

struct CreateMenuContext {
	Scene& scene;
	Entity parent;
	Entity& selected_entity;
	std::optional<PrefabKey>* selected_prefab{ nullptr };

	[[nodiscard]] bool IsCreatingChild() const {
		return static_cast<bool>(parent);
	}
};

using CreateEntityFunction = Entity (*)(Scene&);

void SetCreatedEntityTag(Entity entity, std::string_view tag) {
	if (entity && entity.Has<Tag>()) {
		entity.Get<Tag>().value = tag;
	}
}

void FinalizeCreatedEntity(CreateMenuContext& context, Entity created) {
	if (!created) {
		return;
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

	context.selected_entity = created;
	if (context.selected_prefab) {
		context.selected_prefab->reset();
	}
}

void DrawCreateMenuItem(
	CreateMenuContext& context, const char* label, CreateEntityFunction create,
	bool can_create_as_child = true
) {
	bool disabled{ context.IsCreatingChild() && !can_create_as_child };

	ImGui::BeginDisabled(disabled);

	if (ImGui::MenuItem(label)) {
		FinalizeCreatedEntity(context, create(context.scene));
	}

	ImGui::EndDisabled();
}

Entity CreateDefaultSprite(Scene& scene) {
	return CreateSprite(scene);
}

Entity CreateDefaultAnimation(Scene& scene) {
	return CreateAnimation(scene);
}

Entity CreateDefaultParticleEmitter(Scene& scene) {
	return CreateParticleEmitter(scene, {}, {}, true);
}

Entity CreateDefaultLight(Scene& scene) {
	return CreateLight(scene);
}

Entity CreateDefaultRenderTarget(Scene& scene) {
	return CreateRenderTarget(scene);
}

Entity CreateDefaultCamera(Scene& scene) {
	return CreateCamera(scene);
}

Entity CreateDefaultCustomShader(Scene& scene) {
	return CreateCustomShader(scene);
}

template <typename T>
Entity CreateDefaultEffect(Scene& scene) {
	return CreateEffect<T>(scene);
}

Entity CreateDefaultRect(Scene& scene) {
	return CreateRect(scene, {}, kDefaultShapeSize, color::White);
}

Entity CreateDefaultCircle(Scene& scene) {
	return CreateCircle(scene, {}, kDefaultShapeRadius, color::White);
}

Entity CreateDefaultLine(Scene& scene) {
	return CreateLine(scene, {}, { -100, -100 }, { 100, 100 }, color::White);
}

Entity CreateDefaultPolygon(Scene& scene) {
	return CreatePolygon(
		scene, {},
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

Entity CreateDefaultEllipse(Scene& scene) {
	return CreateEllipse(
		scene, {}, { kDefaultShapeRadius * 2, kDefaultShapeRadius }, color::White
	);
}

Entity CreateDefaultArc(Scene& scene) {
	return CreateArc(scene, {}, kDefaultShapeRadius, 0.0f, 90.0f, true, color::White);
}

Entity CreateDefaultRoundedRect(Scene& scene) {
	return CreateRoundedRect(scene, {}, kDefaultShapeSize, 10.0f, color::White);
}

Entity CreateDefaultTriangle(Scene& scene) {
	return CreateTriangle(scene, {}, { -100, 50 }, { 0, -50 }, { 100, 50 }, color::White);
}

Entity CreateDefaultCapsule(Scene& scene) {
	return CreateCapsule(
		scene, {}, { -100, -100 }, { 100, 100 }, kDefaultShapeRadius, color::White
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
		case InteractivePreset::Draggable: SetDraggable(entity); break;
		case InteractivePreset::Dropzone: SetDropzone(entity); break;
	}
}

template <InteractivePreset Preset>
Entity CreateDefaultInteractiveRect(Scene& scene) {
	auto entity{ CreateRect(scene, {}, kDefaultShapeSize, color::White) };
	auto shape{ scene.CreateEntity() };
	shape.Add<Rect>(kDefaultShapeSize);
	AddInteractiveShape(entity, shape);
	ApplyInteractivePreset(entity, Preset);
	return entity;
}

template <InteractivePreset Preset>
Entity CreateDefaultInteractiveCircle(Scene& scene) {
	auto entity{ CreateCircle(scene, {}, kDefaultShapeRadius, color::White) };
	auto shape{ scene.CreateEntity() };
	shape.Add<Circle>(kDefaultShapeRadius);
	AddInteractiveShape(entity, shape);
	ApplyInteractivePreset(entity, Preset);
	return entity;
}

template <InteractivePreset Preset>
Entity CreateDefaultInteractiveSprite(Scene& scene) {
	auto entity{ CreateSprite(scene) };
	auto shape{ scene.CreateEntity() };
	shape.Add<Rect>(kDefaultInteractiveSpriteSize);
	AddInteractiveShape(entity, shape);
	ApplyInteractivePreset(entity, Preset);
	return entity;
}

void ConfigureDefaultButton(Button button, std::string_view text) {
	button.Background().Color(color::Black.WithAlpha(180));
	button.Text().Content(text).Color(color::White);
}

Entity CreateDefaultUIButton(Scene& scene) {
	auto button{ CreateButton(scene, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(button, "Button");
	SetCreatedEntityTag(button, "Button");
	return button;
}

Entity CreateDefaultUISpriteButton(Scene& scene) {
	auto button{ CreateButton(scene, {}, kDefaultUISquareButtonSize) };
	button.Background().Color(color::Black.WithAlpha(96));
	button.Sprite().Size(kDefaultUISpriteSize);
	SetCreatedEntityTag(button, "Sprite Button");
	return button;
}

Entity CreateDefaultUIAnimatedButton(Scene& scene) {
	auto button{ CreateButton(scene, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(button, "Animated Button");
	button.Animation().Size(kDefaultUISpriteSize);
	SetCreatedEntityTag(button, "Animated Button");
	return button;
}

Entity CreateDefaultUIToggleButton(Scene& scene) {
	auto button{ CreateToggleButton(scene, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(button, "Toggle");
	SetCreatedEntityTag(button, "Toggle Button");
	return button;
}

Entity CreateDefaultUIToggleGroup(Scene& scene) {
	auto group{ CreateToggleButtonGroup(scene) };
	SetCreatedEntityTag(group, "Toggle Group");
	return group;
}

Entity CreateDefaultUIDropdown(Scene& scene) {
	auto dropdown{ CreateDropdown(scene, {}, kDefaultUIButtonSize) };
	ConfigureDefaultButton(dropdown, "Dropdown");

	auto first_item{ dropdown.AddItem("Item 1") };
	auto second_item{ dropdown.AddItem("Item 2") };
	ConfigureDefaultButton(first_item, "Item 1");
	ConfigureDefaultButton(second_item, "Item 2");

	SetCreatedEntityTag(dropdown, "Dropdown");
	return dropdown;
}

Entity CreateDefaultUIText(Scene& scene) {
	auto text{ CreateText(scene, {}, "Default Text", color::White) };
	SetCreatedEntityTag(text, "Text");
	return text;
}

Entity CreateDefaultUIPanel(Scene& scene) {
	auto panel{ CreateRect(scene, {}, kDefaultUIPanelSize, color::Black.WithAlpha(180)) };
	SetCreatedEntityTag(panel, "Panel");
	return panel;
}

std::string GetUniqueTooltipName(Scene& scene) {
	std::string name{ "Tooltip" };
	std::size_t suffix{ 2 };

	while (Tooltip::Get(scene, name).has_value()) {
		name = "Tooltip " + std::to_string(suffix++);
	}

	return name;
}

Entity CreateDefaultUITooltip(Scene& scene) {
	auto name{ GetUniqueTooltipName(scene) };

	TooltipProperties properties;
	properties.content = "Tooltip";

	auto tooltip{ CreateTooltip(scene, name, properties) };
	SetCreatedEntityTag(tooltip, name);
	return tooltip;
}

Entity CreateDefaultUIDialogueBox(Scene& scene) {
	DialogueDesc desc;
	desc.box_size = kDefaultUIDialogueSize;

	auto dialogue_box{ CreateDialogueBox(scene, {}, desc) };
	SetCreatedEntityTag(dialogue_box, "Dialogue Box");
	return dialogue_box;
}

void DrawEffectsCreateMenu(CreateMenuContext& context) {
	if (!ImGui::BeginMenu("Effects")) {
		return;
	}

	DrawCreateMenuItem(context, "Bloom", CreateDefaultEffect<Bloom>);
	DrawCreateMenuItem(context, "Blur", CreateDefaultEffect<Blur>);
	DrawCreateMenuItem(context, "Gaussian Blur", CreateDefaultEffect<GaussianBlur>);
	DrawCreateMenuItem(context, "Grayscale", CreateDefaultEffect<Grayscale>);
	DrawCreateMenuItem(context, "Inverse Color", CreateDefaultEffect<InverseColor>);
	DrawCreateMenuItem(context, "Sharpen", CreateDefaultEffect<Sharpen>);
	DrawCreateMenuItem(context, "Edge Detection", CreateDefaultEffect<EdgeDetection>);

	ImGui::EndMenu();
}

void DrawShapesCreateMenu(CreateMenuContext& context) {
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

void DrawInteractivePresetItems(CreateMenuContext& context, InteractivePreset preset) {
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

void DrawInteractiveCreateMenu(CreateMenuContext& context) {
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

void DrawUICreateMenu(CreateMenuContext& context) {
	if (!ImGui::BeginMenu("UI")) {
		return;
	}

	DrawCreateMenuItem(context, "Button", CreateDefaultUIButton);
	DrawCreateMenuItem(context, "Sprite Button", CreateDefaultUISpriteButton);
	DrawCreateMenuItem(context, "Animated Button", CreateDefaultUIAnimatedButton);
	DrawCreateMenuItem(context, "Toggle Button", CreateDefaultUIToggleButton);
	DrawCreateMenuItem(context, "Toggle Group", CreateDefaultUIToggleGroup);
	DrawCreateMenuItem(context, "Dropdown", CreateDefaultUIDropdown);
	DrawCreateMenuItem(context, "Text", CreateDefaultUIText);
	DrawCreateMenuItem(context, "Panel", CreateDefaultUIPanel);
	DrawCreateMenuItem(context, "Tooltip", CreateDefaultUITooltip);
	DrawCreateMenuItem(context, "Dialogue Box", CreateDefaultUIDialogueBox);

	ImGui::EndMenu();
}

path GetPrefabProjectRoot(const path& scene_path) {
	if (scene_path.empty()) {
		return GetWorkingDirectory();
	}

	path directory{ scene_path.parent_path() };
	for (path current{ directory }; !current.empty();) {
		if (current.filename() == "Scenes") {
			auto root{ current.parent_path() };
			return root.empty() ? GetWorkingDirectory() : root;
		}

		auto parent{ current.parent_path() };
		if (parent == current) {
			break;
		}
		current = std::move(parent);
	}

	return directory.empty() ? GetWorkingDirectory() : directory;
}

PrefabKey MakeUniquePrefabKey(AssetManager& assets, std::string_view display_name) {
	std::string base{ MakePrefabKey(display_name).value };
	PrefabKey key{ base };
	std::size_t suffix{ 2 };

	while (assets.Has(key) || assets.HasCatalogAsset(key)) {
		key = PrefabKey{ base + "_" + std::to_string(suffix++) };
	}

	return key;
}

void AddDefaultPrefabTransform(PrefabEntity& entity) {
	const auto* component{ ComponentRegistry::Find<Transform>() };
	if (!component || !component->make_default_json || !IsPrefabComponentSupported(*component)) {
		return;
	}

	json value = component->make_default_json();
	if (value.is_null()) {
		value = json::object();
	}

	entity.components.emplace_back(PrefabComponent{
		.type = std::string{ component->name },
		.value = std::move(value),
	});
}

PrefabKey SaveNewPrefab(
	AssetManager& assets, const path& project_root, Prefab prefab
) {
	PrefabKey key{ prefab.key };
	auto source_path{ GetPrefabSourcePath(key) };
	assets.SavePrefab(std::move(prefab), project_root / source_path, source_path);
	return key;
}

PrefabKey CreateBlankPrefab(AssetManager& assets, const path& project_root) {
	Prefab prefab;
	prefab.key = MakeUniquePrefabKey(assets, "New Prefab");
	prefab.root.tag = "New Prefab";
	AddDefaultPrefabTransform(prefab.root);
	return SaveNewPrefab(assets, project_root, std::move(prefab));
}

PrefabKey CreatePrefabFromEntity(
	AssetManager& assets, const path& project_root, Entity entity
) {
	std::string name{ entity.Has<Tag>() ? entity.Get<Tag>().value : std::string{ "Prefab" } };
	auto key{ MakeUniquePrefabKey(assets, name) };
	return SaveNewPrefab(assets, project_root, CapturePrefab(entity, key, true));
}

void DrawPrefabCreateMenu(CreateMenuContext& context) {
	if (!ImGui::BeginMenu("Prefab")) {
		return;
	}

	auto& assets{ context.scene.ctx().asset };
	auto keys{ assets.GetPrefabKeys() };
	for (const auto& key : keys) {
		std::string label{ key.value };
		if (label.starts_with(kPrefabKeyPrefix)) {
			label.erase(0, kPrefabKeyPrefix.size());
		}

		if (ImGui::MenuItem(label.c_str())) {
			FinalizeCreatedEntity(context, context.scene.CreatePrefab(key));
		}
	}

	if (keys.empty()) {
		ImGui::TextDisabled("No prefabs");
	}

	ImGui::EndMenu();
}

void DrawCreateEntityMenu(
	Scene& scene, Entity parent, Entity& selected_entity,
	std::optional<PrefabKey>* selected_prefab = nullptr
) {
	CreateMenuContext context{
		.scene{ scene },
		.parent{ parent },
		.selected_entity{ selected_entity },
		.selected_prefab = selected_prefab,
	};

	bool creating_child{ context.IsCreatingChild() };
	const char* create_entity_label{ creating_child ? "Create Child Entity" : "Create Entity" };
	const char* create_submenu_label{ creating_child ? "Create Child" : "Create" };

	auto child_acceptance_reason{ parent ? GetChildAcceptanceLockReason(parent)
									 : std::optional<std::string_view>{} };
	bool can_create_child{ !child_acceptance_reason.has_value() };

	if (ImGui::MenuItem(create_entity_label, nullptr, false, can_create_child)) {
		FinalizeCreatedEntity(context, scene.CreateEntity());
	}

	if (!ImGui::BeginMenu(create_submenu_label, can_create_child)) {
		return;
	}

	DrawCreateMenuItem(context, "Sprite", CreateDefaultSprite);
	DrawCreateMenuItem(context, "Animation", CreateDefaultAnimation);
	DrawCreateMenuItem(context, "Particle Emitter", CreateDefaultParticleEmitter);
	DrawCreateMenuItem(context, "Light", CreateDefaultLight);
	DrawCreateMenuItem(context, "Render Target", CreateDefaultRenderTarget);

	// Cameras must remain at the scene root.
	DrawCreateMenuItem(context, "Camera", CreateDefaultCamera, false);

	DrawCreateMenuItem(context, "Custom Shader", CreateDefaultCustomShader);

	DrawEffectsCreateMenu(context);
	DrawShapesCreateMenu(context);
	DrawInteractiveCreateMenu(context);
	DrawUICreateMenu(context);
	DrawPrefabCreateMenu(context);

	ImGui::EndMenu();
}

void DrawEntitiesTab(
	EditorContext& ctx, Scene& scene, const path& project_root,
	Entity& selected_entity, std::optional<PrefabKey>& selected_prefab,
	SceneHierarchyTab& active_tab, bool& select_active_tab, std::array<char, 256>& filter
) {
	Entity entity_to_delete;
	PendingHierarchyDrop pending_drop;

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint(
		"##HierarchyFilter", "Filter: player, -enemy, *hidden, *shown", filter.data(),
		filter.size()
	);

	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();

		ImGui::TextUnformatted("Hierarchy filter syntax:");
		ImGui::Separator();

		ImGui::TextUnformatted("player");
		ImGui::SameLine();
		ImGui::TextDisabled("Name contains \"player\"");

		ImGui::TextUnformatted("-enemy");
		ImGui::SameLine();
		ImGui::TextDisabled("Name does not contain \"enemy\"");

		ImGui::TextUnformatted("*shown");
		ImGui::SameLine();
		ImGui::TextDisabled("Entity is visible");

		ImGui::TextUnformatted("*hidden");
		ImGui::SameLine();
		ImGui::TextDisabled("Entity is hidden");

		ImGui::Spacing();
		ImGui::TextDisabled("Separate filters with commas.");
		ImGui::TextDisabled("Positive name filters use OR; all other filters must match.");

		ImGui::EndTooltip();
	}

	ImGui::Separator();

	auto filter_text{ std::string_view{ filter.data() } };

	bool entity_left_clicked_this_frame{ false };

	auto draw_entity = [&](auto&& self, Entity entity, std::size_t recursion_depth) -> void {
		PTGN_ASSERT(
			recursion_depth <= kMaxParentDepth,
			"Maximum parent depth exceeded while sorting entities by depth. "
			"This likely indicates a cycle in the entity hierarchy"
		);

		if (!EntityOrDescendantMatchesFilter(entity, filter_text)) {
			return;
		}

		bool selected{ entity == selected_entity };
		std::vector<Entity> children;

		if (HasChildren(entity)) {
			children = GetChildren(entity);
		}

		children.erase(
			std::remove_if(
				children.begin(), children.end(),
				[&](Entity child) { return !EntityOrDescendantMatchesFilter(child, filter_text); }
			),
			children.end()
		);
		SortByLocalDepth(children);

		bool has_visible_children{ !children.empty() };

		ImGui::PushID(entity.Get<UUID>());

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
		bool invalid_entity_drop_target{ hierarchy_drag_active && !can_drop_on_entity };

		if (invalid_entity_drop_target) {
			constexpr ImVec4 kTransparent{ 0.0f, 0.0f, 0.0f, 0.0f };

			ImGui::PushStyleColor(ImGuiCol_Header, kTransparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kTransparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, kTransparent);
		}

		auto label{ entity.Get<Tag>() };
		bool open{ ImGui::TreeNodeEx("##Entity", flags, "%s", label.value.c_str()) };

		if (invalid_entity_drop_target) {
			ImGui::PopStyleColor(3);
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			selected_entity			   = entity;
			selected_prefab.reset();
			entity_left_clicked_this_frame = true;
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			selected_entity = entity;
			selected_prefab.reset();
		}

		auto hierarchy_restriction_reason{ GetHierarchyRestrictionReason(entity) };
		auto deletion_lock_reason{ GetDeletionLockReason(entity) };
		auto child_acceptance_reason{ GetChildAcceptanceLockReason(entity) };

		if (ImGui::BeginDragDropSource()) {
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

		if (ImGui::BeginPopupContextItem()) {
			DrawCreateEntityMenu(scene, entity, selected_entity, &selected_prefab);

			if (ImGui::MenuItem("Save As Prefab")) {
				selected_prefab = CreatePrefabFromEntity(
					scene.ctx().asset, project_root, entity
				);
				selected_entity = {};
				active_tab = SceneHierarchyTab::Prefabs;
				select_active_tab = true;
				ctx.local.state.is_dirty = true;
			}

			ImGui::Separator();

			if (ptgn::HasParent(entity)) {
				bool can_move_to_root{ CanReparent(entity, {}) };

				ImGui::BeginDisabled(!can_move_to_root);

				if (ImGui::MenuItem("Move To Root")) {
					pending_drop = PendingHierarchyDrop{
						.entity{ entity },
						.parent{},
					};
				}

				ImGui::EndDisabled();
			}

			ImGui::BeginDisabled(deletion_lock_reason.has_value());

			if (ImGui::MenuItem("Delete")) {
				entity_to_delete = entity;
			}

			ImGui::EndDisabled();

			DrawRestrictionReasons(
				{
					child_acceptance_reason,
					hierarchy_restriction_reason,
					deletion_lock_reason,
				}
			);

			ImGui::EndPopup();
		}

		if (has_visible_children && open) {
			for (Entity child : children) {
				self(self, child, recursion_depth + 1);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	};

	auto roots{ GetSiblings(scene, {}) };

	for (Entity entity : roots) {
		draw_entity(draw_entity, entity, 0);
	}

	Entity dragged{ GetDraggedEntity(scene) };
	bool can_drop_at_root{ dragged && ptgn::HasParent(dragged) && CanReparent(dragged, {}) };

	if (can_drop_at_root) {
		ImVec2 available{ ImGui::GetContentRegionAvail() };

		if (available.x > 0.0f && available.y > 0.0f) {
			ImGui::InvisibleButton("##HierarchyRootDropTarget", available);

			if (ImGui::BeginDragDropTarget()) {
				if (Entity dropped{ AcceptDraggedEntity(scene) };
					dropped && ptgn::HasParent(dropped) && CanReparent(dropped, {})) {
					pending_drop = PendingHierarchyDrop{
						.entity{ dropped },
						.parent{},
					};
				}

				ImGui::EndDragDropTarget();
			}
		}
	}

	if (ImGui::BeginPopupContextWindow(
			"SceneHierarchyPanelContextMenu",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems
		)) {
		// Comment this if right clicking to open the popup context should not deselect the current
		// entity.
		selected_entity = {};

		DrawCreateEntityMenu(scene, {}, selected_entity, &selected_prefab);
		ImGui::EndPopup();
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!entity_left_clicked_this_frame /* && !ImGui::IsAnyItemHovered() */) {
		selected_entity = {};
	}

	if (pending_drop) {
		ApplyHierarchyDrop(pending_drop);
	}

	if (entity_to_delete && !GetDeletionLockReason(entity_to_delete).has_value()) {
		if (selected_entity == entity_to_delete) {
			selected_entity = {};
		}

		entity_to_delete.Destroy();
	}

}

void DrawPrefabsTab(
	EditorContext& ctx, Scene& scene, const path& project_root,
	Entity& selected_entity, std::optional<PrefabKey>& selected_prefab,
	SceneHierarchyTab& active_tab, bool& select_active_tab
) {
	auto& assets{ scene.ctx().asset };

	if (ImGui::Button("+ New Prefab", ImVec2{ -1.0f, 0.0f })) {
		selected_prefab = CreateBlankPrefab(assets, project_root);
		selected_entity = {};
		ctx.local.state.is_dirty = true;
	}

	ImGui::Separator();

	std::optional<PrefabKey> prefab_to_delete;
	std::optional<PrefabKey> prefab_to_duplicate;

	for (const auto& key : assets.GetPrefabKeys()) {
		bool selected{ selected_prefab.has_value() && selected_prefab.value() == key };
		std::string label{ key.value };
		if (label.starts_with(kPrefabKeyPrefix)) {
			label.erase(0, kPrefabKeyPrefix.size());
		}

		if (ImGui::Selectable(label.c_str(), selected)) {
			selected_prefab = key;
			selected_entity = {};
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			selected_entity = scene.CreatePrefab(key);
			selected_prefab.reset();
			active_tab = SceneHierarchyTab::Entities;
			select_active_tab = true;
		}

		if (!ImGui::BeginPopupContextItem()) {
			continue;
		}

		if (ImGui::MenuItem("Create Instance")) {
			selected_entity = scene.CreatePrefab(key);
			selected_prefab.reset();
			active_tab = SceneHierarchyTab::Entities;
			select_active_tab = true;
		}

		if (ImGui::MenuItem("Duplicate")) {
			prefab_to_duplicate = key;
		}

		if (ImGui::MenuItem("Delete")) {
			prefab_to_delete = key;
		}

		ImGui::EndPopup();
	}

	if (prefab_to_duplicate.has_value()) {
		auto source{ impl::AssetAccessor{ assets }.Get<Prefab>(prefab_to_duplicate.value()) };
		Prefab copy{ source.get() };
		copy.key = MakeUniquePrefabKey(assets, copy.root.tag);
		selected_prefab = SaveNewPrefab(assets, project_root, std::move(copy));
		ctx.local.state.is_dirty = true;
	}

	if (prefab_to_delete.has_value()) {
		assets.RemovePrefab(prefab_to_delete.value());
		if (selected_prefab == prefab_to_delete) {
			selected_prefab.reset();
		}
		ctx.local.state.is_dirty = true;
	}
}

} // namespace

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow");

	const auto& scene_list{ ctx.editor.GetSceneListPanel() };
	auto* selected_scene{ scene_list.GetSelectedScene() };

	if (!selected_scene) {
		ImGui::End();
		return;
	}

	auto project_root{ GetPrefabProjectRoot(scene_list.GetSelectedScenePath()) };

	if (ImGui::BeginTabBar("##SceneHierarchyTabs")) {
		auto entity_flags{ select_active_tab_ && active_tab_ == SceneHierarchyTab::Entities
			? ImGuiTabItemFlags_SetSelected
			: ImGuiTabItemFlags_None };
		if (ImGui::BeginTabItem("Entities", nullptr, entity_flags)) {
			if (select_active_tab_ && active_tab_ == SceneHierarchyTab::Entities) {
				select_active_tab_ = false;
			}
			active_tab_ = SceneHierarchyTab::Entities;
			DrawEntitiesTab(
				ctx, *selected_scene, project_root, selected_entity_, selected_prefab_,
				active_tab_, select_active_tab_, filter_
			);
			ImGui::EndTabItem();
		}

		auto prefab_flags{ select_active_tab_ && active_tab_ == SceneHierarchyTab::Prefabs
			? ImGuiTabItemFlags_SetSelected
			: ImGuiTabItemFlags_None };
		if (ImGui::BeginTabItem("Prefabs", nullptr, prefab_flags)) {
			if (select_active_tab_ && active_tab_ == SceneHierarchyTab::Prefabs) {
				select_active_tab_ = false;
			}
			active_tab_ = SceneHierarchyTab::Prefabs;
			DrawPrefabsTab(
				ctx, *selected_scene, project_root, selected_entity_, selected_prefab_, active_tab_,
				select_active_tab_
			);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
	return selected_entity_;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
	selected_entity_ = entity;
	selected_prefab_.reset();
	active_tab_ = SceneHierarchyTab::Entities;
	select_active_tab_ = true;
}

const std::optional<PrefabKey>& SceneHierarchyPanel::GetSelectedPrefab() const {
	return selected_prefab_;
}

void SceneHierarchyPanel::SetSelectedPrefab(std::optional<PrefabKey> prefab) {
	selected_prefab_ = std::move(prefab);
	selected_entity_ = {};
	active_tab_ = SceneHierarchyTab::Prefabs;
	select_active_tab_ = true;
}

} // namespace ptgn::editor
