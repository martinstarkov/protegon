#include "panels/scene_hierarchy.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <cstdint>
#include <functional>
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
#include "panels/scene_list.h"
#include "runtime/animation/animation.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
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
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

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

} // namespace

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow");

	const auto& scene_list{ ctx.editor.GetSceneListPanel() };
	auto selected_scene{ scene_list.GetSelectedScene() };

	if (!selected_scene) {
		ImGui::End();
		return;
	}

	Entity entity_to_delete;
	PendingHierarchyDrop pending_drop;

	auto select_created_entity = [&](Entity entity) {
		if (!entity) {
			return;
		}

		selected_entity_ = entity;
	};

	auto draw_create_entity_menu = [&](Entity parent = {}) {
		bool creating_child{ static_cast<bool>(parent) };

		const char* create_entity_label{ creating_child ? "Create Child Entity" : "Create Entity" };
		const char* create_submenu_label{ creating_child ? "Create Child" : "Create" };

		auto finalize_created_entity = [&](Entity created) {
			if (!created) {
				return;
			}

			if (parent) {
				// This is a final safeguard for entities such as cameras that are
				// not allowed to become children.
				if (!CanReparent(created, parent)) {
					PTGN_WARN(
						"Could not create entity as a child of ", parent.Get<Tag>().value,
						" because the hierarchy relationship is restricted"
					);
					created.Destroy();
					return;
				}

				SetParent(created, parent);
			}

			select_created_entity(created);
		};

		auto create_menu_item = [&](const char* label, auto&& create,
									bool can_create_as_child = true) {
			bool disabled{ creating_child && !can_create_as_child };

			ImGui::BeginDisabled(disabled);

			if (ImGui::MenuItem(label)) {
				finalize_created_entity(std::invoke(std::forward<decltype(create)>(create)));
			}

			ImGui::EndDisabled();
		};

		auto draw_submenu = [&](const char* label, auto&& draw_contents) {
			if (!ImGui::BeginMenu(label)) {
				return;
			}

			std::invoke(std::forward<decltype(draw_contents)>(draw_contents));

			ImGui::EndMenu();
		};

		auto child_acceptance_reason{ parent ? GetChildAcceptanceLockReason(parent)
											 : std::optional<std::string_view>{} };

		bool can_create_child{ !child_acceptance_reason.has_value() };

		if (ImGui::MenuItem(create_entity_label, nullptr, false, can_create_child)) {
			finalize_created_entity(selected_scene->CreateEntity());
		}

		if (ImGui::BeginMenu(create_submenu_label, can_create_child)) {
			create_menu_item("Sprite", [&]() { return CreateSprite(*selected_scene); });

			create_menu_item("Animation", [&]() { return CreateAnimation(*selected_scene); });

			create_menu_item("Particle Emitter", [&]() {
				return CreateParticleEmitter(*selected_scene, {}, {}, true);
			});

			create_menu_item("Light", [&]() { return CreateLight(*selected_scene); });

			create_menu_item("Text", [&]() {
				return CreateText(*selected_scene, {}, "Default Text", color::White);
			});

			create_menu_item("Render Target", [&]() {
				return CreateRenderTarget(*selected_scene);
			});

			// Cameras must remain at the scene root.
			create_menu_item("Camera", [&]() { return CreateCamera(*selected_scene); }, false);

			create_menu_item("Custom Shader", [&]() {
				return CreateCustomShader(*selected_scene);
			});

			draw_submenu("Effects", [&]() {
				create_menu_item("Bloom", [&]() { return CreateEffect<Bloom>(*selected_scene); });

				create_menu_item("Blur", [&]() { return CreateEffect<Blur>(*selected_scene); });

				create_menu_item("Gaussian Blur", [&]() {
					return CreateEffect<GaussianBlur>(*selected_scene);
				});

				create_menu_item("Grayscale", [&]() {
					return CreateEffect<Grayscale>(*selected_scene);
				});

				create_menu_item("Inverse Color", [&]() {
					return CreateEffect<InverseColor>(*selected_scene);
				});

				create_menu_item("Sharpen", [&]() {
					return CreateEffect<Sharpen>(*selected_scene);
				});

				create_menu_item("Edge Detection", [&]() {
					return CreateEffect<EdgeDetection>(*selected_scene);
				});
			});

			draw_submenu("Shapes", [&]() {
				constexpr auto kShapeColor{ color::White };
				constexpr V2_float kShapeSize{ 100, 100 };
				constexpr float kShapeRadius{ 50 };

				create_menu_item("Rect", [&]() {
					return CreateRect(*selected_scene, {}, kShapeSize, kShapeColor);
				});

				create_menu_item("Circle", [&]() {
					return CreateCircle(*selected_scene, {}, kShapeRadius, kShapeColor);
				});

				create_menu_item("Line", [&]() {
					return CreateLine(
						*selected_scene, {}, { -100, -100 }, { 100, 100 }, kShapeColor
					);
				});

				create_menu_item("Polygon", [&]() {
					return CreatePolygon(
						*selected_scene, {},
						{
							{ 0, -50 },
							{ 47, -15 },
							{ 29, 40 },
							{ -29, 40 },
							{ -47, -15 },
						},
						kShapeColor
					);
				});

				create_menu_item("Ellipse", [&]() {
					return CreateEllipse(
						*selected_scene, {}, { kShapeRadius * 2, kShapeRadius }, kShapeColor
					);
				});

				create_menu_item("Arc", [&]() {
					return CreateArc(
						*selected_scene, {}, kShapeRadius, 0.0f, 90.0f, true, kShapeColor
					);
				});

				create_menu_item("Rounded Rect", [&]() {
					return CreateRoundedRect(*selected_scene, {}, kShapeSize, 10.0f, kShapeColor);
				});

				create_menu_item("Triangle", [&]() {
					return CreateTriangle(
						*selected_scene, {}, { -100, 50 }, { 0, -50 }, { 100, 50 }, kShapeColor
					);
				});

				create_menu_item("Capsule", [&]() {
					return CreateCapsule(
						*selected_scene, {}, { -100, -100 }, { 100, 100 }, kShapeRadius, kShapeColor
					);
				});
			});

			ImGui::EndMenu();
		}
	};

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint(
		"##HierarchyFilter", "Filter: player, -enemy, *hidden, *shown", filter_.data(),
		filter_.size()
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

	auto filter_text{ std::string_view{ filter_.data() } };

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

		bool selected{ entity == selected_entity_ };
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

		Entity dragged{ GetDraggedEntity(*selected_scene) };

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
			selected_entity_			   = entity;
			entity_left_clicked_this_frame = true;
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			selected_entity_ = entity;
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
			if (Entity dropped{ AcceptDraggedEntity(*selected_scene) };
				dropped && CanReparent(dropped, entity)) {
				pending_drop = PendingHierarchyDrop{
					.entity{ dropped },
					.parent{ entity },
				};
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem()) {
			draw_create_entity_menu(entity);

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

	auto roots{ GetSiblings(*selected_scene, {}) };

	for (Entity entity : roots) {
		draw_entity(draw_entity, entity, 0);
	}

	Entity dragged{ GetDraggedEntity(*selected_scene) };
	bool can_drop_at_root{ dragged && ptgn::HasParent(dragged) && CanReparent(dragged, {}) };

	if (can_drop_at_root) {
		ImVec2 available{ ImGui::GetContentRegionAvail() };

		if (available.x > 0.0f && available.y > 0.0f) {
			ImGui::InvisibleButton("##HierarchyRootDropTarget", available);

			if (ImGui::BeginDragDropTarget()) {
				if (Entity dropped{ AcceptDraggedEntity(*selected_scene) };
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
		selected_entity_ = {};

		draw_create_entity_menu();
		ImGui::EndPopup();
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!entity_left_clicked_this_frame /* && !ImGui::IsAnyItemHovered() */) {
		selected_entity_ = {};
	}

	if (pending_drop) {
		ApplyHierarchyDrop(pending_drop);
	}

	if (entity_to_delete && !GetDeletionLockReason(entity_to_delete).has_value()) {
		if (selected_entity_ == entity_to_delete) {
			selected_entity_ = {};
		}

		entity_to_delete.Destroy();
	}

	ImGui::End();
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
	return selected_entity_;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
	selected_entity_ = entity;
}

} // namespace ptgn::editor
