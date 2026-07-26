#include "panels/scene_list.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] bool IsRuntimeSceneList(const Editor& editor) {
	return editor.IsPlaying() || editor.IsDirectRuntime();
}

[[nodiscard]] bool IsVisibleScene(const Scene& scene, bool runtime_scene_list) {
	return scene.IsRuntime() == runtime_scene_list;
}

[[nodiscard]] std::string GetSceneTypeDisplayName(const Scene& scene) {
	if (scene.GetRegisteredType().empty()) {
		return "Scene";
	}

	return impl::GetSceneRegistration(scene.GetRegisteredType()).display_name;
}

[[nodiscard]] SerializedScene MakeNewProjectScene(
	const impl::SceneRegistryEntry& registration
) {
	return SerializedScene{
		.type = registration.type,
		.parameters = registration.default_parameters(),
		.assets = {},
		.content = std::nullopt,
	};
}

void DrawRegisteredSceneTypes(EditorContext& ctx) {
	if (!ImGui::CollapsingHeader("Registered Scene Types")) {
		return;
	}

	std::vector<const impl::SceneRegistryEntry*> registrations;
	registrations.reserve(impl::GetSceneRegistry().size());

	for (const auto& [scene_type, registration] : impl::GetSceneRegistry()) {
		(void)scene_type;
		registrations.emplace_back(&registration);
	}

	std::ranges::sort(
		registrations,
		{},
		[](const impl::SceneRegistryEntry* registration) {
			return registration->display_name;
		}
	);

	if (registrations.empty()) {
		ImGui::TextDisabled("No registered scene types");
		return;
	}

	for (const auto* registration : registrations) {
		PTGN_ASSERT(registration);

		ImGui::PushID(registration->type.c_str());
		ImGui::TextUnformatted(registration->display_name.c_str());
		ImGui::SameLine();

		if (ImGui::SmallButton("Create Project Scene")) {
			ctx.editor.CreateProjectScene(
				registration->display_name,
				MakeNewProjectScene(*registration)
			);
		}

		ImGui::PopID();
	}
}

} // namespace

void SceneListPanel::DrawSceneParamUI(EditorContext& ctx) {
	if (pending_scene_selection_.has_value()) {
		ImGui::SeparatorText("Scene Parameters");
		ImGui::TextDisabled("Loading scene...");
		return;
	}

	if (!selected_scene_ || !state_.has_value()) {
		return;
	}

	auto& state{ state_.value() };
	const bool runtime{ selected_scene_->IsRuntime() };

	ImGui::SeparatorText("Scene Parameters");
	ImGui::Text("Type: %s", state.display_name.c_str());
	ImGui::Text("Scene Name: %s", selected_scene_->GetTag().c_str());
	ImGui::TextDisabled(runtime ? "Runtime instance" : "Project scene");

	for (auto it{ state.params.begin() }; it != state.params.end(); ++it) {
		DrawJsonEditor(it.key().c_str(), it.value());
	}

	if (!ImGui::Button("Apply Parameters")) {
		return;
	}

	std::optional<UUID> selected_entity_uuid;
	Entity selected_entity{
		ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity()
	};

	if (selected_entity &&
		&selected_entity.GetScene() == selected_scene_) {
		selected_entity_uuid = selected_entity.Get<UUID>();
	}

	std::string target_scene_tag{ selected_scene_->GetTag() };
	SerializedScene serialized_scene{ CaptureScene(*selected_scene_) };

	serialized_scene.type = state.scene_type;
	serialized_scene.parameters = state.params;

	auto scene_factory{
		impl::MakeSceneFactory(
			std::move(serialized_scene),
			runtime
		)
	};

	if (!ctx.editor.GetSceneManager().ReEnterFactory(
			target_scene_tag,
			std::move(scene_factory)
		)) {
		return;
	}

	QueueSceneSelection(
		ctx,
		std::move(target_scene_tag),
		runtime,
		selected_entity_uuid
	);
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
		scene
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
	const bool runtime_scene_list{ IsRuntimeSceneList(ctx.editor) };

	auto select_scene = [&](
		Scene* scene,
		std::optional<UUID> selected_entity_uuid = std::nullopt
	) {
		SetSelectedScene(ctx, scene);

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
				pending_selection.selected_entity_uuid
			);
		}
	}

	bool selected_scene_exists{
		selected_scene_ &&
		std::ranges::any_of(
			scenes,
			[this, runtime_scene_list](const auto& scene) {
				return scene &&
					   scene.get() == selected_scene_ &&
					   IsVisibleScene(*scene, runtime_scene_list);
			}
		)
	};

	if (!selected_scene_exists && selected_scene_) {
		// Do not pass the stale or hidden pointer to Editor::OnSelectedSceneChanged().
		selected_scene_ = nullptr;
		state_.reset();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({});
	}

	if (!selected_scene_ && !pending_scene_selection_.has_value()) {
		auto it{ std::ranges::find_if(
			scenes,
			[runtime_scene_list](const auto& scene) {
				return scene && IsVisibleScene(*scene, runtime_scene_list);
			}
		) };

		select_scene(it == scenes.end() ? nullptr : it->get());
	}

	ImGui::SeparatorText(
		runtime_scene_list
			? "Active Runtime Scenes"
			: "Project Scenes"
	);

	bool requested_delete{ false };

	for (const auto& scene : scenes) {
		PTGN_ASSERT(scene);

		if (!IsVisibleScene(*scene, runtime_scene_list)) {
			continue;
		}

		bool selected{ scene.get() == selected_scene_ };
		std::string tag{
			scene->GetTag().empty()
				? "Untitled Scene"
				: scene->GetTag()
		};
		std::string type_name{ GetSceneTypeDisplayName(*scene) };

		std::string visible_label{ tag };
		if (!type_name.empty() && type_name != tag) {
			visible_label += " (" + type_name + ")";
		}

		if (!runtime_scene_list &&
			ctx.editor.IsStartupProjectScene(scene->GetTag())) {
			visible_label += " [Startup]";
		}

		if (scene->IsTransitioning()) {
			visible_label += " [Transitioning]";
		}

		std::string selectable_label{
			visible_label + "###Scene" +
			std::to_string(scene->GetTagHash())
		};

		if (ImGui::Selectable(selectable_label.c_str(), selected)) {
			select_scene(scene.get());
		}

		if (runtime_scene_list || !ImGui::BeginPopupContextItem()) {
			continue;
		}

		const bool startup{
			ctx.editor.IsStartupProjectScene(scene->GetTag())
		};

		if (ImGui::MenuItem(
				"Set as Startup Scene",
				nullptr,
				startup,
				!startup
			)) {
			ctx.editor.SetStartupProjectScene(scene->GetTag());
		}

		if (ImGui::MenuItem("Delete", nullptr, false, !startup)) {
			requested_delete = ctx.editor.DeleteProjectScene(scene->GetTag());
		}

		ImGui::EndPopup();

		if (requested_delete) {
			break;
		}
	}

	if (!runtime_scene_list) {
		DrawRegisteredSceneTypes(ctx);

		if (ImGui::BeginPopupContextWindow(
				"ScenesContext",
				ImGuiPopupFlags_MouseButtonRight |
					ImGuiPopupFlags_NoOpenOverItems
			)) {
			if (ImGui::BeginMenu("Add Project Scene")) {
				std::vector<const impl::SceneRegistryEntry*> registrations;
				registrations.reserve(impl::GetSceneRegistry().size());

				for (const auto& [scene_type, registration] :
					 impl::GetSceneRegistry()) {
					(void)scene_type;
					registrations.emplace_back(&registration);
				}

				std::ranges::sort(
					registrations,
					{},
					[](const impl::SceneRegistryEntry* registration) {
						return registration->display_name;
					}
				);

				for (const auto* registration : registrations) {
					PTGN_ASSERT(registration);

					if (!ImGui::MenuItem(
							registration->display_name.c_str()
						)) {
						continue;
					}

					ctx.editor.CreateProjectScene(
						registration->display_name,
						MakeNewProjectScene(*registration)
					);

					ImGui::CloseCurrentPopup();
					break;
				}

				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}
	}

	DrawSceneParamUI(ctx);

	ImGui::End();
}

Scene* SceneListPanel::GetSelectedScene() const {
	return selected_scene_;
}

void SceneListPanel::QueueSceneSelection(
	EditorContext& ctx,
	std::string scene_tag,
	bool runtime,
	std::optional<UUID> selected_entity_uuid
) {
	pending_scene_selection_ = PendingSceneSelection{
		.tag = std::move(scene_tag),
		.runtime = runtime,
		.selected_entity_uuid =
			std::move(selected_entity_uuid),
		.earliest_frame = ImGui::GetFrameCount() + 1,
	};

	SetSelectedScene(ctx, nullptr);
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({});
}

void SceneListPanel::SetSelectedScene(EditorContext& ctx, Scene* scene) {
	if (selected_scene_ == scene) {
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
	state_.reset();

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
