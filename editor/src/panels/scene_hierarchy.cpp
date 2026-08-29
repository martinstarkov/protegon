#include "panels/scene_hierarchy.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/scene_list.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
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
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
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

	return false;
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

std::optional<std::string_view> GetDuplicationLockReason(Entity entity) {
	if (auto reason{ GetDeletionLockReason(entity) }) {
		return reason;
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
	Entity entity, std::string_view filter_text, bool show_managed_ui_parts,
	std::size_t recursion_depth = 0
) {
	PTGN_ASSERT(
		recursion_depth <= kMaxParentDepth,
		"Maximum parent depth exceeded while filtering entities. "
		"This likely indicates a cycle in the entity hierarchy"
	);

	// Managed UI parts are implementation details of their owning control. When they are hidden,
	// exclude the entire managed branch from both hierarchy drawing and filter matching.
	if (!show_managed_ui_parts && IsManagedButtonVisual(entity)) {
		return false;
	}

	if (EntityMatchesFilter(entity, filter_text)) {
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
	EditorContext& ctx;
	Scene& scene;
	Entity parent{};
	Entity& selected_entity;

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

	context.selected_entity =
		context.ctx.commands.RecordCreatedEntity(created, context.ctx.local.selection);
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

Entity CreateDefaultSpriteStack(Scene& scene) {
	return CreateSpriteStack(scene);
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
	return CreateEllipse(scene, {}, { kDefaultShapeRadius * 2, kDefaultShapeRadius }, color::White);
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
		case InteractivePreset::Draggable:	 SetDraggable(entity); break;
		case InteractivePreset::Dropzone:	 SetDropzone(entity); break;
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
		FinalizeCreatedEntity(context, entity);
	}

	if (!has_creatable_effect) {
		ImGui::TextDisabled("No creatable effects are registered.");
	}

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
		const std::string name{ component->name };

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

	prefab.key = MakeUniquePrefabKey(assets, project_root, "New Prefab");

	prefab.root.tag = "New Prefab";

	AddDefaultPrefabSpatialComponents(prefab.root);

	return prefab;
}

Prefab CreatePrefabFromEntity(const AssetManager& assets, const path& project_root, Entity entity) {
	std::string name{ entity.Has<Tag>() ? entity.Get<Tag>().value : std::string{ "Prefab" } };

	const auto key{ MakeUniquePrefabKey(assets, project_root, name) };

	return CapturePrefab(entity, key, true);
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
	EditorContext& ctx, Scene& scene, Entity parent, Entity& selected_entity
) {
	CreateMenuContext context{
		.ctx{ ctx },
		.scene{ scene },
		.parent{ parent },
		.selected_entity{ selected_entity },
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
	DrawCreateMenuItem(context, "Sprite Stack", CreateDefaultSpriteStack);
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

	if (renaming_entity.has_value() && (renaming_entity->scene_key != scene.GetTag() ||
										renaming_entity->runtime != scene.IsRuntime() ||
										!renaming_entity->entity_uuid.has_value() ||
										!scene.GetEntity(renaming_entity->entity_uuid.value()))) {
		renaming_entity.reset();
		entity_rename_text.clear();
		entity_rename_error.clear();
		focus_entity_rename = false;
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint(
		"##HierarchyFilter", "Filter: player, -enemy, *hidden, *shown", filter.data(), filter.size()
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

		if (!EntityOrDescendantMatchesFilter(entity, filter_text, show_managed_ui_parts)) {
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
				[&](Entity child) {
					return !EntityOrDescendantMatchesFilter(
						child, filter_text, show_managed_ui_parts
					);
				}
			),
			children.end()
		);
		SortByLocalDepth(children);

		bool has_visible_children{ !children.empty() };

		const UUID entity_uuid{ entity.Get<UUID>() };
		ImGui::PushID(entity_uuid);

		if (renaming_entity.has_value() && renaming_entity->scene_key == scene.GetTag() &&
			renaming_entity->runtime == scene.IsRuntime() &&
			renaming_entity->entity_uuid == entity_uuid) {
			if (focus_entity_rename) {
				ImGui::SetKeyboardFocusHere();
				focus_entity_rename = false;
			}

			ImGui::SetNextItemWidth(-FLT_MIN);

			const bool submitted{ ImGui::InputText(
				"##RenameEntity", &entity_rename_text,
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
			) };

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				entity_left_clicked_this_frame = true;
			}

			const bool cancel{ ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape) };

			const bool commit{ submitted || ImGui::IsItemDeactivated() };

			if (cancel) {
				renaming_entity.reset();
				entity_rename_text.clear();
				entity_rename_error.clear();
				focus_entity_rename = false;
			} else if (commit) {
				auto name{ TrimWhitespace(entity_rename_text) };

				if (name.empty()) {
					entity_rename_error = "Entity name cannot be empty.";
					focus_entity_rename = true;
				} else {
					if (name != entity.Get<Tag>().value) {
						entity_to_rename   = entity;
						entity_rename_name = std::move(name);
					}

					renaming_entity.reset();
					entity_rename_text.clear();
					entity_rename_error.clear();
					focus_entity_rename = false;
				}
			}

			if (!entity_rename_error.empty()) {
				ImGui::TextDisabled("%s", entity_rename_error.c_str());
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
			selected_entity				   = entity;
			entity_left_clicked_this_frame = true;
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			selected_entity = entity;
		}

		auto hierarchy_restriction_reason{ GetHierarchyRestrictionReason(entity) };
		auto duplication_lock_reason{ GetDuplicationLockReason(entity) };
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
			DrawCreateEntityMenu(ctx, scene, entity, selected_entity);

			if (ImGui::MenuItem("Rename")) {
				renaming_entity = SceneEntitySelection{
					.scene_key	 = scene.GetTag(),
					.runtime	 = scene.IsRuntime(),
					.entity_uuid = entity_uuid,
				};
				entity_rename_text = entity.Get<Tag>().value;
				entity_rename_error.clear();
				focus_entity_rename = true;
			}

			ImGui::BeginDisabled(duplication_lock_reason.has_value());

			if (ImGui::MenuItem("Duplicate")) {
				entity_to_duplicate = entity;
			}

			ImGui::EndDisabled();

			ImGui::BeginDisabled(!project_root.has_value());

			if (ImGui::MenuItem("Save As Prefab")) {
				const PrefabKey created_key{ ctx.commands.CreatePrefabAsset(
					CreatePrefabFromEntity(scene.ctx().asset, project_root.value(), entity)
				) };

				if (!created_key.value.empty()) {
					selected_prefab = created_key;

					ImGui::SetWindowFocus("Prefabs###PrefabsWindow");
				}
			}

			ImGui::EndDisabled();

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
					duplication_lock_reason,
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

		DrawCreateEntityMenu(ctx, scene, {}, selected_entity);
		ImGui::EndPopup();
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!entity_left_clicked_this_frame /* && !ImGui::IsAnyItemHovered() */) {
		selected_entity = {};
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
		if (selected_entity == entity_to_delete) {
			selected_entity = {};
		}

		ctx.commands.DeleteEntity(entity_to_delete);
	}
}

} // namespace

bool SceneHierarchyPanel::DrawSceneHierarchy(EditorContext& ctx) {
	const bool visible{ ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow") };

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
			focus_entity_rename_, show_managed_ui_parts_
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
	const bool visible{ ImGui::Begin("Prefabs###PrefabsWindow") };

	if (visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		SetActiveTab(SceneHierarchyTab::Prefabs);
	}

	auto& assets{ ctx.editor.GetAssetManager() };

	const auto& scene_list{ ctx.editor.GetSceneListPanel() };

	auto* selected_scene{ scene_list.GetSelectedScene() };

	const auto project_root{ ctx.editor.GetProjectRoot() };

	ImGui::BeginDisabled(!project_root.has_value());

	if (ImGui::Button("+ New Prefab", ImVec2{ -1.0f, 0.0f })) {
		(void)ctx.commands.CreatePrefabAsset(CreateBlankPrefab(assets, project_root.value()));
	}

	ImGui::EndDisabled();
	ImGui::Separator();

	enum class PrefabEntityOperationType {
		AddChild,
		Duplicate,
		Delete
	};

	struct PendingPrefabEntityOperation {
		PrefabEntityOperationType type{};
		PrefabKey key{};
		SerializedEntityPath path{};
	};

	std::optional<PrefabKey> prefab_to_delete{};
	std::optional<PrefabKey> prefab_to_duplicate{};
	std::optional<std::pair<PrefabKey, PrefabKey>> prefab_to_rename{};
	std::optional<PendingPrefabEntityOperation> entity_operation{};

	bool prefab_left_clicked_this_frame{ false };

	for (const auto& key : assets.GetPrefabKeys()) {
		ImGui::PushID(key.value.c_str());

		auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(key) };

		auto& prefab{ prefab_asset.get() };

		auto draw_prefab_entity = [&](auto&& self, SerializedEntity& entity,
									  SerializedEntityPath entity_path, bool root) -> void {
			std::string entity_id{ "root" };
			for (const std::size_t index : entity_path) {
				entity_id += "/" + std::to_string(index);
			}

			ImGui::PushID(entity_id.c_str());

			const bool selected{ GetSelectedPrefab() == key &&
								 GetSelectedPrefabEntityPath() == entity_path };

			const bool has_children{ !entity.children.empty() };

			if (root && renaming_prefab_ == key) {
				if (focus_prefab_rename_) {
					ImGui::SetKeyboardFocusHere();
					focus_prefab_rename_ = false;
				}

				ImGui::SetNextItemWidth(-FLT_MIN);

				const bool submitted{ ImGui::InputText(
					"##RenamePrefab", &prefab_rename_text_,
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
				) };

				const bool cancel{ ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape) };

				const bool commit{ submitted || ImGui::IsItemDeactivatedAfterEdit() };

				if (cancel) {
					renaming_prefab_.reset();
					prefab_rename_text_.clear();
					prefab_rename_error_.clear();
				} else if (commit) {
					auto display_name{ TrimWhitespace(prefab_rename_text_) };

					if (display_name.empty()) {
						prefab_rename_error_ = "Prefab name cannot be empty.";
					} else {
						PrefabKey new_key{ MakePrefabKey(display_name) };

						if (!project_root.has_value()) {
							prefab_rename_error_ = "No project path is available.";
						} else if (!IsPrefabKeyAvailable(
									   assets, project_root.value(), key, new_key
								   )) {
							prefab_rename_error_ = "A prefab with that key already exists.";
						} else {
							if (new_key != key) {
								prefab_to_rename = std::pair{ key, std::move(new_key) };
							}

							renaming_prefab_.reset();
							prefab_rename_text_.clear();
							prefab_rename_error_.clear();
						}
					}
				}

				if (!prefab_rename_error_.empty()) {
					ImGui::TextDisabled("%s", prefab_rename_error_.c_str());
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

			const std::string label{ root ? GetPrefabDisplayName(key) : entity.tag };

			if (force_open_prefab_ == key && force_open_prefab_entity_path_ == entity_path) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
				force_open_prefab_.reset();
				force_open_prefab_entity_path_.clear();
			}

			const bool open{ ImGui::TreeNodeEx("##PrefabEntity", flags, "%s", label.c_str()) };

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				SetSelectedPrefab(key, entity_path);
				prefab_left_clicked_this_frame = true;
			}

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				SetSelectedPrefab(key, entity_path, false);
			}

			if (root && selected_scene && ImGui::IsItemHovered() &&
				ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				(void)ctx.commands.CreatePrefabInstance(*selected_scene, key);

				ImGui::SetWindowFocus("Scene Hierarchy###SceneHierarchyWindow");
			}

			if (ImGui::BeginPopupContextItem("PrefabEntityContextMenu")) {
				if (ImGui::MenuItem("Add Child")) {
					entity_operation = PendingPrefabEntityOperation{
						.type = PrefabEntityOperationType::AddChild,
						.key  = key,
						.path = entity_path,
					};
				}

				if (root) {
					ImGui::Separator();
					ImGui::BeginDisabled(!selected_scene);

					if (ImGui::MenuItem("Create Instance")) {
						(void)ctx.commands.CreatePrefabInstance(*selected_scene, key);

						ImGui::SetWindowFocus("Scene Hierarchy###SceneHierarchyWindow");
					}

					ImGui::EndDisabled();
					ImGui::BeginDisabled(!project_root.has_value());

					if (ImGui::MenuItem("Rename Prefab")) {
						SetSelectedPrefab(key, {}, false);
						renaming_prefab_	= key;
						prefab_rename_text_ = label;
						prefab_rename_error_.clear();
						focus_prefab_rename_ = true;
					}

					if (ImGui::MenuItem("Duplicate Prefab")) {
						prefab_to_duplicate = key;
					}

					ImGui::EndDisabled();

					if (ImGui::MenuItem("Delete Prefab")) {
						prefab_to_delete = key;
					}
				} else {
					ImGui::Separator();

					if (ImGui::MenuItem("Duplicate Entity")) {
						entity_operation = PendingPrefabEntityOperation{
							.type = PrefabEntityOperationType::Duplicate,
							.key  = key,
							.path = entity_path,
						};
					}

					if (ImGui::MenuItem("Delete Entity")) {
						entity_operation = PendingPrefabEntityOperation{
							.type = PrefabEntityOperationType::Delete,
							.key  = key,
							.path = entity_path,
						};
					}
				}

				ImGui::EndPopup();
			}

			if (has_children && open) {
				for (std::size_t i{ 0 }; i < entity.children.size(); ++i) {
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
	}

	if (prefab_to_rename.has_value()) {
		const auto& [old_key, new_key]{ prefab_to_rename.value() };

		if (!ctx.commands.RenamePrefabAsset(old_key, new_key)) {
			renaming_prefab_	 = old_key;
			prefab_rename_text_	 = GetPrefabDisplayName(new_key);
			prefab_rename_error_ = "Failed to rename prefab.";
			focus_prefab_rename_ = true;
		}
	}

	if (prefab_to_duplicate.has_value() && project_root.has_value()) {
		auto source{
			::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_to_duplicate.value())
		};

		const PrefabKey duplicate_key{
			MakeUniquePrefabKey(assets, project_root.value(), source.get().root.tag)
		};

		(void)ctx.commands.DuplicatePrefabAsset(prefab_to_duplicate.value(), duplicate_key);
	}

	if (entity_operation.has_value()) {
		switch (entity_operation->type) {
			case PrefabEntityOperationType::AddChild: {
				SerializedEntity child;
				child.tag = "Entity";
				AddDefaultPrefabSpatialComponents(child);

				if (ctx.commands.AddPrefabChild(
						entity_operation->key, entity_operation->path, std::move(child)
					)) {
					force_open_prefab_			   = entity_operation->key;
					force_open_prefab_entity_path_ = entity_operation->path;
				}

				break;
			}

			case PrefabEntityOperationType::Duplicate:
				(void)ctx.commands.DuplicatePrefabEntity(
					entity_operation->key, entity_operation->path
				);
				break;

			case PrefabEntityOperationType::Delete:
				(void)ctx.commands.DeletePrefabEntity(
					entity_operation->key, entity_operation->path
				);
				break;
		}
	}

	if (prefab_to_delete.has_value()) {
		if (ctx.commands.DeletePrefabAsset(prefab_to_delete.value())) {
			if (renaming_prefab_ == prefab_to_delete) {
				renaming_prefab_.reset();
				prefab_rename_text_.clear();
				prefab_rename_error_.clear();
			}
		}
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!prefab_left_clicked_this_frame && !ImGui::IsAnyItemHovered()) {
		SetSelectedPrefab(std::nullopt);
	}

	ImGui::End();
	return visible;
}

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	const bool scene_hierarchy_visible{ DrawSceneHierarchy(ctx) };
	const bool prefabs_visible{ DrawPrefabs(ctx) };

	if (scene_hierarchy_visible != prefabs_visible) {
		SetActiveTab(
			scene_hierarchy_visible ? SceneHierarchyTab::SceneHierarchy : SceneHierarchyTab::Prefabs
		);
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
	selection.selected_prefab			  = std::move(prefab);
	selection.selected_prefab_entity_path = std::move(entity_path);
	selection.mode						  = EditorSelectionMode::Prefabs;

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

bool SceneHierarchyPanel::GetShowManagedUIParts() const {
	return show_managed_ui_parts_;
}

void SceneHierarchyPanel::SetShowManagedUIParts(bool show) {
	show_managed_ui_parts_ = show;
}

} // namespace ptgn::editor
