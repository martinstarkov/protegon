#include "panels/screen_effects.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_selection.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
#include "runtime/graphics/visible.h"

namespace ptgn::editor {

namespace {

struct ScreenEffectDragPayload {
	std::size_t index{ 0 };
	bool runtime{ false };
};

void SelectScreenEffect(
	EditorContext& ctx,
	ScreenEffectSelection selection,
	bool undoable = true
) {
	EditorSelection editor_selection{ ctx.local.selection };
	editor_selection.scene_list_tab = SceneListTab::ScreenEffects;
	editor_selection.inspector_tab = InspectorTab::ScreenEffect;
	editor_selection.selected_screen_effect = selection;

	if (undoable) {
		SetEditorSelection(
			ctx,
			std::move(editor_selection),
			"Select Screen Effect",
			true,
			ctx.editor.IsPlaying()
		);
	} else {
		ApplyEditorSelection(ctx, std::move(editor_selection));
	}

	ImGui::SetWindowFocus("Screen Effect Inspector###ScreenEffectInspector");
}

[[nodiscard]] bool IsSelected(
	const EditorContext& ctx,
	ScreenEffectSelection selection
) {
	return ctx.local.selection.selected_screen_effect == selection;
}

void DrawDragSource(
	std::size_t index,
	bool runtime,
	std::string_view label
) {
	if (!ImGui::BeginDragDropSource()) {
		return;
	}

	const ScreenEffectDragPayload payload{
		.index = index,
		.runtime = runtime,
	};

	ImGui::SetDragDropPayload(
		"PTGN_SCREEN_EFFECT_ORDER",
		&payload,
		sizeof(payload)
	);
	ImGui::TextUnformatted(label.data(), label.data() + label.size());
	ImGui::EndDragDropSource();
}

void DrawAddEffectPopup(EditorContext& ctx, bool runtime) {
	if (!ImGui::BeginPopup("AddScreenEffect")) {
		return;
	}

	std::vector<const ::ptgn::impl::RegisteredEffect*> effects;
	effects.reserve(::ptgn::impl::EffectRegistry::Entries().size());

	for (const auto& effect : ::ptgn::impl::EffectRegistry::Entries()) {
		effects.emplace_back(&effect);
	}

	std::ranges::sort(effects, {}, [](const auto* effect) {
		return effect->display_name;
	});

	bool has_creatable_effect{ false };

	for (const auto* effect : effects) {
		if (!effect || !effect->create || !effect->make_default) {
			continue;
		}

		has_creatable_effect = true;

		if (!ImGui::MenuItem(effect->display_name.c_str())) {
			continue;
		}

		if (runtime) {
			ctx.editor.AddRuntimeScreenEffect(effect->type_name);
		} else {
			ctx.editor.AddProjectScreenEffect(effect->type_name);
		}

		ImGui::CloseCurrentPopup();
		break;
	}

	if (!has_creatable_effect) {
		ImGui::TextDisabled("No creatable effects are registered.");
	}

	ImGui::EndPopup();
}

bool DrawProjectEffects(EditorContext& ctx) {
	const auto* settings{ ctx.editor.GetProjectScreenEffects() };
	if (!settings) {
		ImGui::TextDisabled("No project is open.");
		return false;
	}

	bool effect_left_clicked{ false };
	const bool empty{ settings->effects.empty() };
	std::optional<std::pair<std::size_t, std::size_t>> pending_move;
	std::optional<std::pair<ScreenEffectId, bool>> pending_enabled;
	std::optional<ScreenEffectId> pending_duplicate;
	std::optional<ScreenEffectId> pending_delete;

	for (std::size_t index{ 0 }; index < settings->effects.size(); ++index) {
		const auto& effect{ settings->effects[index] };
		const auto* registration{ ::ptgn::impl::EffectRegistry::Find(effect.type) };
		const std::string label{
			registration ? registration->display_name : effect.type + " [Missing]"
		};
		const ScreenEffectSelection selection{
			.id = effect.id,
			.runtime = false,
		};

		ImGui::PushID(static_cast<int>(effect.id));

		bool enabled{ effect.enabled };
		if (ImGui::Checkbox("##Enabled", &enabled)) {
			pending_enabled = std::pair{ effect.id, enabled };
		}
		effect_left_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(enabled ? "Disable this effect." : "Enable this effect.");
		}

		ImGui::SameLine();
		std::string selectable_label{ label + "##ScreenEffect" };
		const bool effect_selected{ ImGui::Selectable(
			selectable_label.c_str(),
			IsSelected(ctx, selection),
			ImGuiSelectableFlags_SpanAvailWidth
		) };
		effect_left_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);
		if (effect_selected) {
			SelectScreenEffect(ctx, selection);
		}

		if (ImGui::BeginPopupContextItem("ProjectScreenEffectContext")) {
			if (ImGui::MenuItem(effect.enabled ? "Disable" : "Enable")) {
				pending_enabled = std::pair{ effect.id, !effect.enabled };
			}
			if (ImGui::MenuItem("Duplicate")) {
				pending_duplicate = effect.id;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) {
				pending_delete = effect.id;
			}
			ImGui::EndPopup();
		}

		DrawDragSource(index, false, label);

		if (ImGui::BeginDragDropTarget()) {
			if (const auto* payload{
					ImGui::AcceptDragDropPayload("PTGN_SCREEN_EFFECT_ORDER")
				}) {
				const auto drag{
					*static_cast<const ScreenEffectDragPayload*>(payload->Data)
				};
				if (!drag.runtime && drag.index != index) {
					pending_move = std::pair{ drag.index, index };
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (registration && registration->hdr) {
			ImGui::SameLine();
			ImGui::TextDisabled("HDR");
			effect_left_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);
		}

		ImGui::PopID();
	}

	if (pending_enabled) {
		ctx.editor.SetProjectScreenEffectEnabled(pending_enabled->first, pending_enabled->second);
	}
	if (pending_move) {
		ctx.editor.MoveProjectScreenEffect(pending_move->first, pending_move->second);
	}
	if (pending_duplicate) {
		ctx.editor.DuplicateProjectScreenEffect(*pending_duplicate);
	}
	if (pending_delete) {
		ctx.editor.DeleteProjectScreenEffect(*pending_delete);
	}

	if (empty) {
		ImGui::TextDisabled("No project screen effects.");
	}

	return effect_left_clicked;
}

bool DrawRuntimeEffects(EditorContext& ctx) {
	const auto& effects{ ctx.editor.GetRuntimeScreenEffects() };
	bool effect_left_clicked{ false };
	std::optional<std::pair<std::size_t, std::size_t>> pending_move;
	std::optional<std::uint64_t> pending_duplicate;
	std::optional<std::uint64_t> pending_delete;

	for (std::size_t index{ 0 }; index < effects.size(); ++index) {
		Entity entity{ effects[index] };
		if (!entity) {
			continue;
		}

		const auto* instance{ entity.TryGet<::ptgn::impl::ScreenEffectInstance>() };
		const auto* registration{
			instance ? ::ptgn::impl::EffectRegistry::Find(instance->type) : nullptr
		};
		const std::string label{
			registration
				? registration->display_name
				: instance
					? instance->type + " [Missing]"
					: std::string{ "Runtime Effect" }
		};

		ScreenEffectSelection selection;
		if (instance && instance->source_id != 0) {
			selection.id = instance->source_id;
			selection.runtime = false;
		} else if (instance) {
			selection.id = instance->runtime_id;
			selection.runtime = true;
		}

		ImGui::PushID(static_cast<int>(index));

		bool enabled{ entity.Has<Visible>() && entity.Get<Visible>().visible };
		if (ImGui::Checkbox("##Enabled", &enabled) && instance) {
			ctx.editor.SetRuntimeScreenEffectEnabled(instance->runtime_id, enabled);
		}
		effect_left_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);

		ImGui::SameLine();
		std::string selectable_label{ label + "##RuntimeScreenEffect" };
		const bool effect_selected{ ImGui::Selectable(
			selectable_label.c_str(),
			instance && IsSelected(ctx, selection),
			ImGuiSelectableFlags_SpanAvailWidth
		) };
		effect_left_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);
		if (effect_selected && instance) {
			SelectScreenEffect(ctx, selection);
		}

		if (instance && ImGui::BeginPopupContextItem("RuntimeScreenEffectContext")) {
			if (ImGui::MenuItem(enabled ? "Disable" : "Enable")) {
				ctx.editor.SetRuntimeScreenEffectEnabled(instance->runtime_id, !enabled);
			}
			if (ImGui::MenuItem("Duplicate")) {
				pending_duplicate = instance->runtime_id;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) {
				pending_delete = instance->runtime_id;
			}
			ImGui::EndPopup();
		}

		DrawDragSource(index, true, label);

		if (ImGui::BeginDragDropTarget()) {
			if (const auto* payload{
					ImGui::AcceptDragDropPayload("PTGN_SCREEN_EFFECT_ORDER")
				}) {
				const auto drag{
					*static_cast<const ScreenEffectDragPayload*>(payload->Data)
				};
				if (drag.runtime && drag.index != index) {
					pending_move = std::pair{ drag.index, index };
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (registration && registration->hdr) {
			ImGui::SameLine();
			ImGui::TextDisabled("HDR");
			effect_left_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);
		}

		ImGui::PopID();
	}

	if (pending_move) {
		ctx.editor.MoveRuntimeScreenEffect(pending_move->first, pending_move->second);
	}
	if (pending_duplicate) {
		ctx.editor.DuplicateRuntimeScreenEffect(*pending_duplicate);
	}
	if (pending_delete) {
		ctx.editor.DeleteRuntimeScreenEffect(*pending_delete);
	}

	if (effects.empty()) {
		ImGui::TextDisabled("No runtime screen effects.");
	}

	return effect_left_clicked;
}

} // namespace

void ScreenEffectsPanel::OnRender(EditorContext& ctx) {
	const bool visible{ ImGui::Begin("Screen Effects") };

	if (!visible) {
		ImGui::End();
		return;
	}

	SyncVisibleSceneListTab(ctx, SceneListTab::ScreenEffects);

	const bool runtime{ ctx.editor.IsPlaying() || ctx.editor.IsDirectRuntime() };
	bool preview{ ctx.editor.GetSettings().preview_screen_effects };

	if (ImGui::Checkbox("Preview", &preview)) {
		ctx.editor.SetScreenEffectPreview(preview);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			runtime
				? "Enable or disable screen effects during runtime."
				: "Preview project screen effects in the editor viewport."
		);
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(
		std::max(
			ImGui::GetCursorPosX(),
			ImGui::GetWindowContentRegionMax().x - 105.0f
		)
	);

	if (ImGui::Button("+ Add Effect", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddScreenEffect");
	}
	DrawAddEffectPopup(ctx, runtime);

	ImGui::Separator();

	const bool effect_left_clicked{
		runtime
			? DrawRuntimeEffects(ctx)
			: DrawProjectEffects(ctx)
	};

	if (ctx.local.selection.selected_screen_effect.has_value() &&
		ImGui::IsWindowHovered() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!effect_left_clicked) {
		ClearSelectedScreenEffect(ctx);
	}

	ImGui::End();
}

} // namespace ptgn::editor
