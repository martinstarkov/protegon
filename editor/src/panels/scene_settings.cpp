#include "panels/scene_settings.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "editor/editor.h"
#include "editor/modal_scale.h"
#include "editor/editor_context.h"
#include "panels/inspector_fields.h"
#include "panels/inspector_helpers.h"
#include "panels/scene_list.h"
#include "platform/file_dialog.h"
#include "platform/window.h"
#include "runtime/graphics/render_target.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/world/chunk.h"

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
		.bounds	 = physics.GetBounds(),
	};
}

void ApplyPhysicsSettings(Physics& physics, const PhysicsSettingsState& settings) {
	physics.SetEnabled(settings.enabled);
	physics.SetGravity(settings.gravity);
	physics.SetBounds(settings.bounds);
}

[[nodiscard]] Scene* ResolveScene(Editor& editor, std::string_view scene_key) {
	auto& manager{ editor.GetSceneManager() };
	auto scene_hash{ Hash(scene_key) };

	if (!manager.HasScene(scene_hash)) {
		return nullptr;
	}

	return &manager.GetScene(scene_hash);
}

template <typename T, typename Apply>
void TrackSceneSettingsChange(
	EditorContext& ctx, Scene& scene, std::string_view interaction_id, std::string label,
	bool changed, T before, T after, Apply apply
) {
	if (!changed) {
		return;
	}

	Editor* editor{ &ctx.editor };
	std::string scene_key{ scene.GetTag() };
	std::uint64_t interaction_key{ static_cast<std::uint64_t>(
		ImGui::GetID(interaction_id.data(), interaction_id.data() + interaction_id.size())
	) };

	ctx.undo.TrackInteraction(
		interaction_key, std::move(label), true, ImGui::IsAnyItemActive(),
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
	std::string registered_type{ scene.GetRegisteredType() };
	DrawPropertyRow("Registered Type", [&]() {
		ImGui::TextDisabled("%s", registered_type.c_str());
		return false;
	});

	DrawPropertyRow("Scene State", [&]() {
		ImGui::TextDisabled(
			"%zu entities | %s | render %s | transition %s", scene.GetEntityCount(),
			scene.IsRuntime() ? "runtime" : "editor", scene.IsRenderEnabled() ? "on" : "off",
			scene.IsTransitioning() ? "active" : "none"
		);
		return false;
	});

	auto render_target_size{ scene.GetRenderTarget().GetSize() };
	DrawWHValue("Render Target Size", render_target_size, 1.0f, 0, 0, ImGuiSliderFlags_None, true);

	auto interaction_info{ scene.ctx().interaction.GetDebugInfo() };
	DrawPropertyRow("Interaction State", [&]() {
		ImGui::TextDisabled(
			"%zu cameras | %zu dragging | %zu hovered", interaction_info.tracked_cameras,
			interaction_info.dragging_entities, interaction_info.hovered_entities
		);
		return false;
	});
}

} // namespace

void SceneSettingsPanel::OnRender(EditorContext& ctx) {
	bool visible{ ImGui::Begin("Settings###SceneSettingsWindow") };

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

	std::string scene_key{ scene->GetTag() };
	ImGui::PushID(scene_key.c_str());

	{
		// AutoLabelWidthScope internally pushes an ImGui ID, so it must be
		// destroyed before the scene ID is popped below.
		AutoLabelWidthScope label_width{ "SceneSettings" };

		bool read_only{ scene->IsRuntime() };
		if (read_only) {
			ImGui::TextDisabled("Runtime scene settings are read only.");
		}

		ImGui::BeginDisabled(read_only);

		DrawSectionTitle("Rendering");

		{
			Color before{ scene->GetBackgroundColor() };
			Color after{ before };
			bool changed{ DrawValue(ctx, "Background Color", after) };

			if (changed) {
				scene->SetBackgroundColor(after);
			}

			TrackSceneSettingsChange(
				ctx, *scene, "BackgroundColor", "Change Scene Background Color", changed, before,
				after, [](Scene& target, const Color& value) { target.SetBackgroundColor(value); }
			);
		}

		DrawSectionTitle("Viewport Culling & World Chunks");

		{
			auto& chunks{ scene->ctx().chunks };
			ChunkSettings before{ chunks.GetSettings() };
			ChunkSettings after{ before };
			bool enabled{ after.world_size.IsPositive() };
			bool changed{ DrawValue(ctx, "Enable Chunks", enabled) };
			DrawTooltip("Use explicit chunks for world streaming.");
			if (enabled && !after.world_size.IsPositive()) {
				after.world_size = { 512, 512 };
			}
			if (!enabled) {
				after.world_size = {};
			}

			changed |= DrawValue(ctx, "Render Viewports", after.render_viewports);
			DrawTooltip("Render area in camera viewports (1.0 = visible area).");
			changed |= DrawValue(ctx, "Simulation Viewports", after.simulation_viewports);
			DrawTooltip("Simulation area in camera viewports.");

			if (enabled) {
				changed |=
					DrawWHValue("Chunk Size (World Units)", after.world_size, 1.0f, 1, 65536);
				DrawTooltip("Chunk width and height in world units.");
				after.world_size.x	= std::max(1, after.world_size.x);
				after.world_size.y	= std::max(1, after.world_size.y);
				changed			   |= DrawValue(ctx, "Render Distance", after.render_distance);
				DrawTooltip("Extra chunks loaded around the render area.");
				changed |= DrawValue(ctx, "Simulation Distance", after.simulation_distance);
				DrawTooltip("Extra chunks around the simulation area.");
				changed |= DrawValue(ctx, "Keep Alive Distance", after.keep_alive_distance);
				DrawTooltip("Unload margin beyond the active area.");
			}
			changed |= DrawValue(ctx, "Max Loaded Pages", after.max_loaded_chunks);
			DrawTooltip("Maximum number of loaded chunks/pages.");
			changed |= DrawPropertyRow("Save Directory", [&]() {
				bool directory_changed{ false };
				float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				float browse_width{ ImGui::CalcTextSize("Browse...").x +
					2.0f * ImGui::GetStyle().FramePadding.x };
				float clear_width{ ImGui::CalcTextSize("X").x +
					2.0f * ImGui::GetStyle().FramePadding.x };

				if (ImGui::Button("Browse...", { browse_width, 0.0f })) {
					FileDialog::Options options{};
					if (!after.save_directory.empty()) {
						options.default_path = path{ after.save_directory };
					}

					auto selected{ ctx.editor.GetWindow().file.OpenFolder(options) };
					if (!selected.has_value()) {
						PTGN_WARN("Unable to browse for chunk save directory: ", selected.error());
					} else if (selected->has_value()) {
						std::string filepath{ selected->value().string() };
						if (filepath != after.save_directory) {
							after.save_directory = std::move(filepath);
							directory_changed = true;
						}
					}
				}
				DrawTooltip("Choose a chunk save folder.");

				ImGui::SameLine(0.0f, spacing);
				std::string displayed_directory{ after.save_directory.empty()
					? "No directory selected" : after.save_directory };
				ImGui::SetNextItemWidth(std::max(
					1.0f, ImGui::GetContentRegionAvail().x - clear_width - spacing
				));
				ImGui::InputText(
					"##ChunkSaveDirectory", &displayed_directory, ImGuiInputTextFlags_ReadOnly
				);
				DrawTooltip("Folder for saved chunk changes.");

				ImGui::SameLine(0.0f, spacing);
				ImGui::BeginDisabled(after.save_directory.empty());
				if (ImGui::Button("X##ClearChunkSaveDirectory", { clear_width, 0.0f })) {
					after.save_directory.clear();
					directory_changed = true;
				}
				ImGui::EndDisabled();
				DrawTooltip("Use memory only changes.");

				return directory_changed;
			});
            changed |= DrawPropertyRow("Save Mode", [&]() {
                int mode{ static_cast<int>(after.save_mode) };
                bool selected{ ImGui::Combo(
                    "##SaveMode", &mode, "Manual\0On Unload\0Automatic\0"
                ) };
                if (selected) {
                    after.save_mode = static_cast<ChunkSaveMode>(mode);
                }
                return selected;
            });
			DrawTooltip("Manual, on unload, or periodic autosave.");
            if (after.save_mode == ChunkSaveMode::Automatic) {
                changed |= DrawPropertyRow("Autosave Interval", [&]() {
                    int interval{ static_cast<int>(after.autosave_interval.count()) };
                    bool interval_changed{ ImGui::DragInt(
                        "##AutosaveInterval", &interval, 1.0f, 1, 86400, "%d s",
                        ImGuiSliderFlags_AlwaysClamp
                    ) };
                    if (interval_changed) {
                        after.autosave_interval = seconds{ std::clamp(interval, 1, 86400) };
                    }
                    return interval_changed;
                });
				DrawTooltip("Seconds between autosaves.");
            }
			ImGui::TextDisabled(
				"%zu active pages | %s", chunks.GetLoadedCount(),
				chunks.GetSaveDirectory().empty() ? "In memory deltas"
												  : "Chunk persistence configured"
			);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Viewport culling remains active without chunks.");
			}

			if (changed) {
				chunks.SetSettings(after);
				after = chunks.GetSettings();
			}

			TrackSceneSettingsChange(
				ctx, *scene, "WorldChunks", "Change World Chunk Settings", changed, before, after,
				[](Scene& target, const ChunkSettings& value) {
					target.ctx().chunks.SetSettings(value);
				}
			);
		}

		DrawSectionTitle("Physics");

		{
			auto& physics{ scene->ctx().physics };

			PhysicsSettingsState before{ CapturePhysicsSettings(physics) };

			PhysicsSettingsState after{ before };
			bool changed{ false };

			changed |= DrawValue(ctx, "Enabled", after.enabled);

			changed |= DrawValue(ctx, "Gravity", after.gravity);

			bool use_bounds{ after.bounds.has_value() };

			bool bounds_toggled{ DrawValue(ctx, "Use Bounds", use_bounds) };
			DrawTooltip("Constrain physics bodies to these bounds.");

			if (bounds_toggled) {
				changed = true;

				if (use_bounds) {
					V2_float default_size{ scene->GetRenderTarget().GetSize() };

					default_size.x = std::max(default_size.x, 1.0f);
					default_size.y = std::max(default_size.y, 1.0f);

					after.bounds = Bounds{
						.position = {},
						.size	  = default_size,
						.behavior = BoundaryBehavior::SlideVelocity,
					};
				} else {
					after.bounds.reset();
				}
			}

			if (after.bounds.has_value()) {
				changed |= DrawValue(ctx, "Bounds Position", after.bounds->position);

				if (DrawWHValue("Bounds Size", after.bounds->size, kInspectorSizeDragSpeed)) {
					after.bounds->size.x = std::max(after.bounds->size.x, 0.001f);

					after.bounds->size.y = std::max(after.bounds->size.y, 0.001f);

					changed = true;
				}

				changed |= DrawValue(ctx, "Bounds Behavior", after.bounds->behavior);
			}

			if (changed) {
				ApplyPhysicsSettings(physics, after);
			}

			TrackSceneSettingsChange(
				ctx, *scene, "Physics", "Change Scene Physics Settings", changed, before, after,
				[](Scene& target, const PhysicsSettingsState& value) {
					ApplyPhysicsSettings(target.ctx().physics, value);
				}
			);
		}

		DrawSectionTitle("Interaction");

		{
			auto& interaction{ scene->ctx().interaction };

			bool before{ interaction.IsTopOnly() };

			bool after{ before };

			bool changed{ DrawValue(ctx, "Top Only", after) };

			if (changed) {
				interaction.SetTopOnly(after);
			}

			DrawTooltip(
				"When enabled, only the topmost interactable "
				"under the cursor receives interaction."
			);

			TrackSceneSettingsChange(
				ctx, *scene, "Interaction", "Change Scene Interaction Settings", changed, before,
				after, [](Scene& target, bool value) { target.ctx().interaction.SetTopOnly(value); }
			);
		}

		ImGui::EndDisabled();

		// Persistence actions remain available while playing, unlike the editable
		// scene configuration above.
		DrawSectionTitle("Chunk Save Controls");
		bool persist_changes{ ctx.editor.GetSettings().persist_runtime_chunk_changes };
		if (DrawValue(ctx, "Persist Changes During Play", persist_changes)) {
			auto settings{ ctx.editor.GetSettings() };
			settings.persist_runtime_chunk_changes = persist_changes;
			ctx.editor.SetEditorSettings(settings);
			if (scene->IsRuntime()) {
				scene->ctx().chunks.SetDiskWritesEnabled(persist_changes);
			}
		}
		DrawTooltip("Keep world changes after Stop. Off discards play session edits.");
		bool has_save_directory{ !scene->ctx().chunks.GetSaveDirectory().empty() };
		bool can_write{ ctx.editor.GetSettings().persist_runtime_chunk_changes };
		ImGui::BeginDisabled(!scene->IsRuntime() || !has_save_directory || !can_write);
		if (ImGui::Button("Save Chunk Changes", ImVec2{ -1.0f, 0.0f })) {
			if (!scene->ctx().chunks.SaveConfigured()) {
				ImGui::OpenPopup("Chunk Save Failed");
			}
		}
		ImGui::EndDisabled();
		DrawTooltip("Save modified chunks now.");
		if (ImGui::BeginPopupModal("Chunk Save Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted("Could not save chunk changes. Check the save directory.");
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::Button("Reset Saved Chunk Changes", ImVec2{ -1.0f, 0.0f })) {
			ImGui::OpenPopup("Reset Saved Chunk Changes?");
		}
		DrawTooltip("Delete saved changes and regenerate the world.");
		// Match the shared editor modal proportions, centering and immovability.
		if (const ImGuiViewport* viewport{ ImGui::GetMainViewport() }) {
			ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, { 0.5f, 0.5f });
			float width{ std::min(600.0f * kEditorModalScale, viewport->Size.x * 0.9f) };
			ImGui::SetNextWindowSizeConstraints({ width, 0.0f }, { width, FLT_MAX });
		}
		const ImGuiStyle& style{ ImGui::GetStyle() };
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
			{ style.WindowPadding.x * 1.5f * kEditorModalScale,
			  style.WindowPadding.y * 1.5f * kEditorModalScale });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			{ style.FramePadding.x * 1.4f * kEditorModalScale,
			  style.FramePadding.y * 1.4f * kEditorModalScale });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			{ style.ItemSpacing.x * 1.25f * kEditorModalScale,
			  style.ItemSpacing.y * 1.4f * kEditorModalScale });
		if (ImGui::BeginPopupModal("Reset Saved Chunk Changes?", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse)) {
			ImGui::SetWindowFontScale(1.05f * kEditorModalScale);
			ImGui::TextWrapped("Delete all saved world chunk changes for this scene? "
				"Harvested resources will regenerate. This cannot be undone.");
			ImGui::Spacing();
			float button_width{ (ImGui::GetContentRegionAvail().x -
				ImGui::GetStyle().ItemSpacing.x) * 0.5f };
			if (ImGui::Button("Cancel", { button_width, 0.0f })) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete Changes", { button_width, 0.0f })) {
				if (scene->ctx().chunks.ResetSavedChanges(*scene)) {
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SetWindowFontScale(1.0f);
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(3);

		if (ctx.editor.GetSettings().show_read_only_scene_data) {
			DrawSectionTitle("Read Only Data");
			DrawReadOnlySceneData(*scene);
		}
	}

	ImGui::PopID();

	ImGui::End();
}

} // namespace ptgn::editor
