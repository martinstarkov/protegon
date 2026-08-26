#include "panels/scene_settings.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
#include "panels/scene_list.h"
#include "runtime/graphics/render_target.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

namespace {

using namespace inspector;

struct PhysicsSettingsState {
	bool enabled{ true };
	V2_float gravity{};
	std::optional<Bounds> bounds{};
};

void DrawSectionTitle(std::string_view title) {
	ImGui::Spacing();
	ImGui::TextUnformatted(title.data(), title.data() + title.size());
	ImGui::Separator();
	ImGui::Spacing();
}

[[nodiscard]] PhysicsSettingsState CapturePhysicsSettings(const Physics& physics) {
	return PhysicsSettingsState{
		.enabled = physics.IsEnabled(),
		.gravity = physics.GetGravity(),
		.bounds = physics.GetBounds(),
	};
}

void ApplyPhysicsSettings(Physics& physics, const PhysicsSettingsState& settings) {
	physics.SetEnabled(settings.enabled);
	physics.SetGravity(settings.gravity);
	physics.SetBounds(settings.bounds);
}

[[nodiscard]] Scene* ResolveScene(Editor& editor, std::string_view scene_key) {
	auto& manager{ editor.GetSceneManager() };
	const auto scene_hash{ Hash(scene_key) };

	if (!manager.HasScene(scene_hash)) {
		return nullptr;
	}

	return &manager.GetScene(scene_hash);
}

template <typename T, typename Apply>
void TrackSceneSettingsChange(
	EditorContext& ctx,
	Scene& scene,
	std::string_view interaction_id,
	std::string label,
	bool changed,
	T before,
	T after,
	Apply apply
) {
	if (!changed) {
		return;
	}

	Editor* editor{ &ctx.editor };
	const std::string scene_key{ scene.GetTag() };
	const std::uint64_t interaction_key{
		static_cast<std::uint64_t>(
			ImGui::GetID(
				interaction_id.data(),
				interaction_id.data() + interaction_id.size()
			)
		)
	};

	ctx.undo.TrackInteraction(
		interaction_key,
		std::move(label),
		true,
		ImGui::IsAnyItemActive(),
		[editor, scene_key, apply, before = std::move(before)]() mutable {
			Scene* target{ ResolveScene(*editor, scene_key) };
			if (!target) {
				return;
			}
			std::invoke(apply, *target, before);
		},
		[editor, scene_key, apply, after = std::move(after)]() mutable {
			Scene* target{ ResolveScene(*editor, scene_key) };
			if (!target) {
				return;
			}
			std::invoke(apply, *target, after);
		},
		true
	);
}

void DrawReadOnlySceneData(const Scene& scene) {
	const std::string registered_type{ scene.GetRegisteredType() };
	DrawPropertyRow("Registered Type", [&]() {
		ImGui::TextDisabled("%s", registered_type.c_str());
		return false;
	});

	DrawPropertyRow("Scene State", [&]() {
		ImGui::TextDisabled(
			"%zu entities | %s | render %s | transition %s",
			scene.GetEntityCount(),
			scene.IsRuntime() ? "runtime" : "editor",
			scene.IsRenderEnabled() ? "on" : "off",
			scene.IsTransitioning() ? "active" : "none"
		);
		return false;
	});

	auto render_target_size{ scene.GetRenderTarget().GetSize() };
	DrawWHValue(
		"Render Target Size",
		render_target_size,
		1.0f,
		0,
		0,
		ImGuiSliderFlags_None,
		true
	);

	const auto interaction_info{ scene.ctx().interaction.GetDebugInfo() };
	DrawPropertyRow("Interaction State", [&]() {
		ImGui::TextDisabled(
			"%zu cameras | %zu dragging | %zu hovered",
			interaction_info.tracked_cameras,
			interaction_info.dragging_entities,
			interaction_info.hovered_entities
		);
		return false;
	});
}

} // namespace

void SceneSettingsPanel::OnRender(EditorContext& ctx) {
	const bool visible{ ImGui::Begin("Settings###SceneSettingsWindow") };

	if (!visible) {
		ImGui::End();
		return;
	}

	Scene* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		ImGui::TextDisabled("Select a scene to edit its settings.");
		ImGui::End();
		return;
	}

	const std::string scene_key{ scene->GetTag() };
	ImGui::PushID(scene_key.c_str());

	{
		// AutoLabelWidthScope internally pushes an ImGui ID, so it must be
		// destroyed before the scene ID is popped below.
		AutoLabelWidthScope label_width{ "SceneSettings" };

		const bool read_only{ scene->IsRuntime() };
		if (read_only) {
			ImGui::TextDisabled("Runtime scene settings are read-only.");
		}

		ImGui::BeginDisabled(read_only);

		DrawSectionTitle("Rendering");

		{
			const Color before{ scene->GetBackgroundColor() };
			Color after{ before };
			const bool changed{ DrawValue(ctx, "Background Color", after) };

			if (changed) {
				scene->SetBackgroundColor(after);
			}

			TrackSceneSettingsChange(
				ctx,
				*scene,
				"BackgroundColor",
				"Change Scene Background Color",
				changed,
				before,
				after,
				[](Scene& target, const Color& value) {
					target.SetBackgroundColor(value);
				}
			);
		}

		DrawSectionTitle("Physics");

		{
			auto& physics{ scene->ctx().physics };

			const PhysicsSettingsState before{
				CapturePhysicsSettings(physics)
			};

			PhysicsSettingsState after{ before };
			bool changed{ false };

			changed |= DrawValue(
				ctx,
				"Enabled",
				after.enabled
			);

			changed |= DrawValue(
				ctx,
				"Gravity",
				after.gravity
			);

			bool use_bounds{ after.bounds.has_value() };

			if (DrawValue(
					ctx,
					"Use Bounds",
					use_bounds
				)) {
				changed = true;

				if (use_bounds) {
					V2_float default_size{
						scene->GetRenderTarget().GetSize()
					};

					default_size.x =
						std::max(default_size.x, 1.0f);
					default_size.y =
						std::max(default_size.y, 1.0f);

					after.bounds = Bounds{
						.position = {},
						.size = default_size,
						.behavior =
							BoundaryBehavior::SlideVelocity,
					};
				} else {
					after.bounds.reset();
				}
			}

			if (after.bounds.has_value()) {
				changed |= DrawValue(
					ctx,
					"Bounds Position",
					after.bounds->position
				);

				if (DrawWHValue(
						"Bounds Size",
						after.bounds->size,
						kInspectorSizeDragSpeed
					)) {
					after.bounds->size.x =
						std::max(
							after.bounds->size.x,
							0.001f
						);

					after.bounds->size.y =
						std::max(
							after.bounds->size.y,
							0.001f
						);

					changed = true;
				}

				changed |= DrawValue(
					ctx,
					"Bounds Behavior",
					after.bounds->behavior
				);
			}

			if (changed) {
				ApplyPhysicsSettings(
					physics,
					after
				);
			}

			TrackSceneSettingsChange(
				ctx,
				*scene,
				"Physics",
				"Change Scene Physics Settings",
				changed,
				before,
				after,
				[](
					Scene& target,
					const PhysicsSettingsState& value
				) {
					ApplyPhysicsSettings(
						target.ctx().physics,
						value
					);
				}
			);
		}

		DrawSectionTitle("Interaction");

		{
			auto& interaction{
				scene->ctx().interaction
			};

			const bool before{
				interaction.IsTopOnly()
			};

			bool after{ before };

			const bool changed{
				DrawValue(
					ctx,
					"Top Only",
					after
				)
			};

			if (changed) {
				interaction.SetTopOnly(after);
			}

			DrawTooltip(
				"When enabled, only the topmost interactable "
				"under the cursor receives interaction."
			);

			TrackSceneSettingsChange(
				ctx,
				*scene,
				"Interaction",
				"Change Scene Interaction Settings",
				changed,
				before,
				after,
				[](Scene& target, bool value) {
					target.ctx()
						.interaction
						.SetTopOnly(value);
				}
			);
		}

		ImGui::EndDisabled();

		if (
			ctx.editor
				.GetSettings()
				.show_read_only_scene_data
		) {
			DrawSectionTitle("Read-Only Data");
			DrawReadOnlySceneData(*scene);
		}
	}

	ImGui::PopID();

	ImGui::End();
}

} // namespace ptgn::editor
