#include "panels/scene_list.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "panels/scene_hierarchy.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"

namespace ptgn::editor {

namespace {

std::string MakeUniqueSceneTag(
	const impl::SceneManager& scene_manager,
	std::string_view base_name
) {
	auto is_available = [&](std::string_view tag) {
		return std::ranges::none_of(
			scene_manager.GetScenes(),
			[tag](const auto& scene) {
				return scene && scene->GetTag() == tag;
			}
		);
	};

	std::string base{ base_name.empty() ? "Scene" : std::string{ base_name } };

	if (is_available(base)) {
		return base;
	}

	for (std::size_t index{ 2 };; ++index) {
		std::string candidate{ base + " " + std::to_string(index) };

		if (is_available(candidate)) {
			return candidate;
		}
	}
}

std::optional<SceneEditorState> MakeSceneEditorState(std::string_view scene_type) {
	auto& registry{ impl::GetSceneRegistry() };
	auto it{ registry.find(scene_type) };

	if (it == registry.end()) {
		return std::nullopt;
	}

	const auto& registration{ it->second };

	return SceneEditorState{
		.scene_type = registration.type,
		.display_name = registration.display_name,
		.scene_tag = "Main",
		.params = registration.default_parameters(),
	};
}

void DrawJsonEditor(const char* label, json& value) {
	if (value.is_boolean()) {
		bool v{ value.get<bool>() };
		if (ImGui::Checkbox(label, &v)) {
			value = v;
		}
	} else if (value.is_number_integer()) {
		int v{ value.get<int>() };
		if (ImGui::InputInt(label, &v)) {
			value = v;
		}
	} else if (value.is_number_float()) {
		float v{ value.get<float>() };
		if (ImGui::InputFloat(label, &v)) {
			value = v;
		}
	} else if (value.is_string()) {
		std::string text{ value.get<std::string>() };
		if (ImGui::InputText(label, &text)) {
			value = std::move(text);
		}
	} else if (value.is_object()) {
		if (!ImGui::TreeNode(label)) {
			return;
		}

		for (auto it{ value.begin() }; it != value.end(); ++it) {
			DrawJsonEditor(it.key().c_str(), it.value());
		}

		ImGui::TreePop();
	} else if (value.is_array()) {
		if (!ImGui::TreeNode(label)) {
			return;
		}

		for (auto i{ 0uz }; i < value.size(); ++i) {
			std::string item{ "[" + std::to_string(i) + "]" };
			DrawJsonEditor(item.c_str(), value[i]);
		}

		ImGui::TreePop();
	} else {
		ImGui::TextDisabled("%s: unsupported", label);
	}
}

} // namespace

void SceneListPanel::DrawSceneParamUI(EditorContext& ctx) {
	if (pending_scene_selection_.has_value()) {
		ImGui::SeparatorText("Scene Parameters");
		ImGui::TextDisabled("Loading scene...");
		return;
	}

	if (!state_.has_value()) {
		state_ = MakeSceneEditorState("EditorScene");
	}

	if (!state_.has_value()) {
		return;
	}

	auto& state{ state_.value() };

	ImGui::SeparatorText("Scene Parameters");
	ImGui::Text("Type: %s", state.display_name.c_str());

	if (selected_scene_) {
		ImGui::Text("Scene Name: %s", selected_scene_->GetTag().c_str());
	} else {
		ImGui::InputText("Scene Name", &state.scene_tag);
	}

	for (auto it{ state.params.begin() }; it != state.params.end(); ++it) {
		DrawJsonEditor(it.key().c_str(), it.value());
	}

	std::string button_text{ selected_scene_ ? "Apply Parameters"
											 : "Enter " + state.display_name };

	if (!ImGui::Button(button_text.c_str())) {
		return;
	}

	std::string target_scene_tag;
	SerializedScene serialized_scene;

	if (selected_scene_) {
		target_scene_tag = selected_scene_->GetTag();
		serialized_scene = CaptureScene(*selected_scene_);
	} else {
		target_scene_tag = state.scene_tag;
		serialized_scene = SerializedScene{
			.type = state.scene_type,
			.parameters = state.params,
			.content = std::nullopt,
		};
	}

	serialized_scene.type = state.scene_type;
	serialized_scene.parameters = state.params;

	auto scene_factory{ impl::MakeSceneFactory(std::move(serialized_scene), false) };
	auto& scene_manager{ ctx.editor.GetSceneManager() };

	bool command_accepted{ selected_scene_
		? scene_manager.ReEnterFactory(target_scene_tag, std::move(scene_factory))
		: scene_manager.EnterFactory(target_scene_tag, std::move(scene_factory)) };

	if (!command_accepted) {
		return;
	}

	QueueSceneSelection(ctx, std::move(target_scene_tag), false);
}

bool SceneListPanel::ResolvePendingSceneSelection(EditorContext& ctx) {
	if (!pending_scene_selection_.has_value()) {
		return false;
	}

	if (ImGui::GetFrameCount() <
		pending_scene_selection_->earliest_frame) {
		return false;
	}

	const auto& pending{
		pending_scene_selection_.value()
	};

	auto& scenes{
		ctx.editor.GetSceneManager().GetScenes()
	};

	auto it{
		std::ranges::find_if(
			scenes,
			[&pending](const auto& scene) {
				return scene &&
					   scene->GetTag() == pending.tag &&
					   scene->IsRuntime() == pending.runtime;
			}
		)
	};

	if (it == scenes.end()) {
		return false;
	}

	auto pending_selection{
		std::move(pending_scene_selection_.value())
	};
	pending_scene_selection_.reset();

	auto* scene{ it->get() };

	SetSelectedScene(
		ctx,
		scene,
		pending_selection.scene_path
	);

	Entity selected_entity;

	if (pending_selection.selected_entity_uuid.has_value()) {
		selected_entity = scene->GetEntity(
			pending_selection.selected_entity_uuid.value()
		);
	}

	ctx.editor
		.GetSceneHierarchyPanel()
		.SetSelectedEntity(selected_entity);

	return true;
}

void SceneListPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scenes");

	auto& scenes{ ctx.editor.GetSceneManager().GetScenes() };

	auto select_scene = [&](
		Scene* scene,
		const path& scene_path = path{},
		std::optional<UUID> selected_entity_uuid = std::nullopt
	) {
		SetSelectedScene(ctx, scene, scene_path);

		if (!selected_scene_) {
			return;
		}

		auto& scene_hierarchy{
			ctx.editor.GetSceneHierarchyPanel()
		};
		const auto& entities{
			selected_scene_->Entities()
		};

		auto select_if_present = [&](Entity entity) {
			if (!entity || !entities.Contains(entity)) {
				return false;
			}

			scene_hierarchy.SetSelectedEntity(entity);
			return true;
		};

		if (selected_entity_uuid.has_value() &&
			select_if_present(
				selected_scene_->GetEntity(
					selected_entity_uuid.value()
				)
			)) {
			return;
		}

		auto& scene_ctx{ selected_scene_->ctx() };
		auto render_target{ selected_scene_->GetRenderTarget() };
		auto fixed_camera{ impl::SceneContextAccessor::GetFixedCamera(scene_ctx) };
		auto camera{ scene_ctx.camera };

		auto regular_entity{ entities.FindIf([&](Entity entity) {
			return entity != render_target && entity != fixed_camera && entity != camera;
		}) };

		if (select_if_present(regular_entity)) {
			return;
		}

		if (select_if_present(camera)) {
			return;
		}

		if (select_if_present(fixed_camera)) {
			return;
		}

		select_if_present(render_target);
	};

	if (pending_scene_selection_.has_value() &&
		ImGui::GetFrameCount() >= pending_scene_selection_->earliest_frame) {
		const auto& pending{
			pending_scene_selection_.value()
		};

		auto it{ std::ranges::find_if(
			scenes,
			[&pending](const auto& scene) {
				return scene &&
					   scene->GetTag() == pending.tag &&
					   scene->IsRuntime() == pending.runtime;
			}
		) };

		if (it != scenes.end()) {
			auto pending_selection{
				std::move(
					pending_scene_selection_.value()
				)
			};
			pending_scene_selection_.reset();

			select_scene(
				it->get(),
				pending_selection.scene_path,
				pending_selection.selected_entity_uuid
			);
		}
	}

	bool selected_scene_exists{ selected_scene_ && std::ranges::any_of(
		scenes,
		[this](const auto& scene) {
			return scene.get() == selected_scene_;
		}
	) };

	if (!selected_scene_exists && selected_scene_) {
		// Do not pass the stale pointer to Editor::OnSelectedSceneChanged().
		selected_scene_ = nullptr;
		selected_scene_path_.clear();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({});
	}

	if (!selected_scene_ && !pending_scene_selection_.has_value()) {
		select_scene(scenes.empty() ? nullptr : scenes.front().get());
	}

	for (auto i{ 0uz }; i < scenes.size(); ++i) {
		const auto& scene{ scenes[i] };
		PTGN_ASSERT(scene);

		bool selected{ scene.get() == selected_scene_ };
		auto tag{ scene->GetTag() };
		auto label{ tag.empty() ? "Untitled Scene" : tag.c_str() };

		if (ImGui::Selectable(label, selected)) {
			select_scene(scene.get());
		}

		if (!ImGui::BeginPopupContextItem()) {
			continue;
		}

		if (ImGui::MenuItem("Delete")) {
			if (scene.get() == selected_scene_) {
				SetSelectedScene(ctx, nullptr);
				ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({});
			}

			// Must happen after SetSelectedScene(ctx.editor, nullptr).
			// TODO: Fix.
			// ctx.editor.DeleteScene(i);

			ImGui::EndPopup();
			break;
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupContextWindow(
			"ScenesContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems
		)) {
		if (ImGui::BeginMenu("Add Scene")) {
			auto& scene_manager{ ctx.editor.GetSceneManager() };

			for (const auto& [scene_type, registration] : impl::GetSceneRegistry()) {
				(void)scene_type;

				if (!ImGui::MenuItem(registration.display_name.c_str())) {
					continue;
				}

				std::string scene_tag{ MakeUniqueSceneTag(
					scene_manager,
					registration.display_name
				) };

				SerializedScene serialized_scene{
					.type = registration.type,
					.parameters = registration.default_parameters(),
					.content = std::nullopt,
				};

				bool accepted{ scene_manager.EnterFactory(
					scene_tag,
					impl::MakeSceneFactory(std::move(serialized_scene), false)
				) };

				if (!accepted) {
					continue;
				}

				QueueSceneSelection(ctx, std::move(scene_tag), false);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	DrawSceneParamUI(ctx);

	ImGui::End();
}

Scene* SceneListPanel::GetSelectedScene() const {
	return selected_scene_;
}

const path& SceneListPanel::GetSelectedScenePath() const {
	return selected_scene_path_;
}

void SceneListPanel::QueueSceneSelection(
	EditorContext& ctx,
	std::string scene_tag,
	bool runtime,
	std::optional<UUID> selected_entity_uuid
) {
	path scene_path;

	// Preserve the file path only when this is replacing the currently selected
	// scene rather than entering a separate scene.
	if (selected_scene_ && selected_scene_->GetTag() == scene_tag) {
		scene_path = selected_scene_path_;
	}

	pending_scene_selection_ = PendingSceneSelection{
		.tag = std::move(scene_tag),
		.runtime = runtime,
		.scene_path = std::move(scene_path),
		.selected_entity_uuid =
			std::move(selected_entity_uuid),
		.earliest_frame = ImGui::GetFrameCount() + 1,
	};

	SetSelectedScene(ctx, nullptr);
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({});
}

void SceneListPanel::SetSelectedScene(EditorContext& ctx, Scene* scene, const path& scene_path) {
	if (selected_scene_ == scene) {
		if (!scene_path.empty()) {
			selected_scene_path_ = scene_path;
		}
		return;
	}

	auto* previous_scene{ selected_scene_ };

	bool previous_scene_exists{ previous_scene && std::ranges::any_of(
		ctx.editor.GetSceneManager().GetScenes(),
		[previous_scene](const auto& active_scene) {
			return active_scene.get() == previous_scene;
		}
	) };

	selected_scene_ = scene;
	selected_scene_path_ = scene_path;

	if (selected_scene_ && !selected_scene_->GetRegisteredType().empty()) {
		std::string registered_type{ selected_scene_->GetRegisteredType() };
		const auto& registration{ impl::GetSceneRegistration(registered_type) };

		state_ = SceneEditorState{
			.scene_type = registration.type,
			.display_name = registration.display_name,
			.scene_tag = selected_scene_->GetTag(),
			.params = registration.serialize_parameters(*selected_scene_),
		};
	}

	ctx.editor.OnSelectedSceneChanged(
		previous_scene_exists ? previous_scene : nullptr,
		selected_scene_
	);
}

} // namespace ptgn::editor
