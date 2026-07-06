#include "panels/scene_hierarchy.h"

#include <imgui.h>

#include <algorithm>
#include <compare>
#include <vector>

#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "panels/scene_list.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scripting/script_sequence.h"

namespace ptgn::editor {

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow");

	const auto& scene_list{ ctx.editor.GetSceneListPanel() };
	auto selected_scene{ scene_list.GetSelectedScene() };

	if (!selected_scene) {
		ImGui::End();
		return;
	}

	Entity entity_to_delete;

	auto select_created_entity = [&](Entity entity) {
		if (!entity) {
			return;
		}

		selected_entity_ = entity;
	};

	auto draw_create_entity_menu = [&]() {
		if (ImGui::MenuItem("Create Entity")) {
			select_created_entity(selected_scene->CreateEntity());
		}

		if (ImGui::BeginMenu("Create")) {
			auto create_menu_item = [&](const char* label, auto&& create) {
				if (ImGui::MenuItem(label)) {
					select_created_entity(create());
				}
			};

			// Keep these default arguments matched to your actual factory overloads.
			create_menu_item("Sprite", [&]() { return CreateSprite(*selected_scene); });

			create_menu_item("Animation", [&]() { return CreateAnimation(*selected_scene); });

			create_menu_item("Particle Emitter", [&]() {
				return CreateParticleEmitter(*selected_scene);
			});

			create_menu_item("Light", [&]() { return CreateLight(*selected_scene); });

			create_menu_item("Text", [&]() {
				return CreateText(*selected_scene, {}, "Default Text", color::White);
			});

			create_menu_item("Render Target", [&]() {
				return CreateRenderTarget(*selected_scene);
			});

			create_menu_item("Camera", [&]() { return CreateCamera(*selected_scene); });

			create_menu_item("Custom Shader", [&]() {
				return CreateCustomShader(*selected_scene);
			});

			create_menu_item("Script Sequence", [&]() {
				return CreateScriptSequence(*selected_scene);
			});

			create_menu_item("Tween", [&]() { return CreateTween(*selected_scene); });

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
				return CreateLine(*selected_scene, {}, { -100, -100 }, { 100, 100 }, kShapeColor);
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
				return CreateArc(*selected_scene, {}, kShapeRadius, 0.0f, 90.0f, true, kShapeColor);
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

			ImGui::EndMenu();
		}
	};

	auto draw_entity = [&](auto&& self, Entity entity, std::size_t recursion_depth) -> void {
		PTGN_ASSERT(
			recursion_depth <= kMaxParentDepth,
			"Maximum parent depth exceeded while sorting entities by depth. "
			"This likely indicates a cycle in the entity hierarchy"
		);

		bool selected{ entity == selected_entity_ };
		bool has_children{ HasChildren(entity) };

		ImGui::PushID(static_cast<int>(entity.GetUUID()));

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

		auto label{ entity.GetTag() };
		bool open{ ImGui::TreeNodeEx("##Entity", flags, "%s", label.c_str()) };

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			selected_entity_ = entity;
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete")) {
				entity_to_delete = entity;
			}

			ImGui::EndPopup();
		}

		if (has_children && open) {
			auto children{ GetChildren(entity) };
			SortByDepth(children);

			for (Entity child : children) {
				self(self, child, recursion_depth + 1);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	};

	std::vector<Entity> roots;

	for (auto entity : selected_scene->Entities()) {
		if (!HasParent(entity)) {
			roots.emplace_back(entity);
		}
	}

	SortByDepth(roots);

	for (Entity entity : roots) {
		draw_entity(draw_entity, entity, 0);
	}

	if (ImGui::BeginPopupContextWindow(
			"SceneHierarchyPanelContextMenu",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems
		)) {
		draw_create_entity_menu();
		ImGui::EndPopup();
	}

	if (entity_to_delete) {
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