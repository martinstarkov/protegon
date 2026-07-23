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
	ImGui::SeparatorText("Scene Parameters");

	if (pending_scene_selection_.has_value()) {
		ImGui::TextDisabled("Loading scene...");
		return;
	}

	if (!selected_scene_ || !state_.has_value()) {
		ImGui::TextDisabled("No scene selected.");
		return;
	}

	auto& state{ state_.value() };

	ImGui::Text(
		"Type: %s",
		state.display_name.c_str()
	);

	ImGui::Text(
		"Scene Name: %s",
		selected_scene_->GetTag().c_str()
	);

	if (state.scene_type.empty()) {
		ImGui::TextDisabled(
			"Default scenes have no custom parameters."
		);
		return;
	}

	bool disabled{
		ctx.state.is_playing ||
		selected_scene_->IsRuntime()
	};

	ImGui::BeginDisabled(disabled);

	for (auto it{ state.params.begin() };
		 it != state.params.end();
		 ++it) {
		DrawJsonEditor(
			it.key().c_str(),
			it.value()
		);
	}

	bool apply_parameters{
		ImGui::Button("Apply Parameters")
	};

	ImGui::EndDisabled();

	if (disabled &&
		ImGui::IsItemHovered(
			ImGuiHoveredFlags_AllowWhenDisabled
		)) {
		ImGui::SetTooltip(
			"Stop Play mode before changing scene parameters."
		);
	}

	if (!apply_parameters) {
		return;
	}

	std::string scene_tag{
		selected_scene_->GetTag()
	};

	SerializedScene serialized_scene{
		CaptureScene(*selected_scene_)
	};

	serialized_scene.type = state.scene_type;
	serialized_scene.parameters = state.params;

	if (!ctx.editor.GetSceneManager().ReEnterFactory(
			scene_tag,
			impl::MakeSceneFactory(
				std::move(serialized_scene),
				false
			)
		)) {
		return;
	}

	ctx.state.is_dirty = true;

	QueueSceneSelection(
		ctx.editor,
		std::move(scene_tag),
		false
	);
}

void SceneListPanel::OnRender(
	EditorContext& ctx
) {
	ImGui::Begin("Scenes");

	auto& scenes{
		ctx.editor.GetSceneManager().GetScenes()
	};

	auto select_scene = [&](Scene* scene) {
		SetSelectedScene(
			ctx.editor,
			scene
		);

		if (!selected_scene_) {
			return;
		}

		auto& scene_hierarchy{
			ctx.editor.GetSceneHierarchyPanel()
		};

		const auto& entities{
			selected_scene_->Entities()
		};

		auto select_if_present =
			[&](Entity entity) {
				if (!entity ||
					!entities.Contains(entity)) {
					return false;
				}

				scene_hierarchy.SetSelectedEntity(
					entity
				);

				return true;
			};

		auto& scene_ctx{
			selected_scene_->ctx()
		};

		auto render_target{
			selected_scene_->GetRenderTarget()
		};

		auto fixed_camera{
			impl::SceneContextAccessor::
				GetFixedCamera(scene_ctx)
		};

		auto camera{ scene_ctx.camera };

		auto regular_entity{
			entities.FindIf(
				[&](Entity entity) {
					return
						entity != render_target &&
						entity != fixed_camera &&
						entity != camera;
				}
			)
		};

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
		ImGui::GetFrameCount() >=
			pending_scene_selection_->
				earliest_frame) {
		const auto& pending{
			pending_scene_selection_.value()
		};

		auto it{
			std::ranges::find_if(
				scenes,
				[&pending](
					const auto& scene
				) {
					return
						scene &&
						scene->GetTag() ==
							pending.tag &&
						scene->IsRuntime() ==
							pending.runtime;
				}
			)
		};

		if (it != scenes.end()) {
			pending_scene_selection_.reset();
			select_scene(it->get());
		}
	}

	bool selected_scene_exists{
		selected_scene_ &&
		std::ranges::any_of(
			scenes,
			[this](const auto& scene) {
				return
					scene.get() ==
					selected_scene_;
			}
		)
	};

	if (!selected_scene_exists &&
		selected_scene_) {
		selected_scene_ = nullptr;
		state_.reset();

		ctx.editor
			.GetSceneHierarchyPanel()
			.SetSelectedEntity({});
	}

	if (!selected_scene_ &&
		!pending_scene_selection_.has_value()) {
		select_scene(
			scenes.empty()
				? nullptr
				: scenes.front().get()
		);
	}

	for (const auto& scene : scenes) {
		PTGN_ASSERT(scene);

		bool selected{
			scene.get() == selected_scene_
		};

		std::string tag{ scene->GetTag() };

		std::string label{
			tag.empty()
				? "Untitled Scene"
				: tag
		};

		bool startup{
			ctx.editor.IsStartupProjectScene(tag)
		};

		if (startup) {
			label += " [Startup]";
		}

		if (ImGui::Selectable(
				label.c_str(),
				selected
			)) {
			select_scene(scene.get());
		}

		if (!ImGui::BeginPopupContextItem()) {
			continue;
		}

		if (ImGui::MenuItem(
				"Set as Startup Scene",
				nullptr,
				startup,
				!startup &&
					!ctx.state.is_playing
			)) {
			ctx.editor.SetStartupProjectScene(
				tag
			);
		}

		if (ImGui::MenuItem(
				"Delete",
				nullptr,
				false,
				!startup &&
					!ctx.state.is_playing
			)) {
			ctx.editor.DeleteProjectScene(tag);

			ImGui::EndPopup();
			break;
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupContextWindow(
			"ScenesContext",
			ImGuiPopupFlags_MouseButtonRight |
				ImGuiPopupFlags_NoOpenOverItems
		)) {
		if (ImGui::BeginMenu("Add Scene")) {
			if (ImGui::MenuItem("Scene")) {
				ctx.editor.CreateProjectScene(
					"Scene",
					SerializedScene{
						.type =
							std::string{
								impl::kBaseSceneType
							},
						.parameters =
							json::object(),
						.assets = {},
						.content =
							std::nullopt,
					}
				);

				ImGui::CloseCurrentPopup();
			}

			if (!impl::GetSceneRegistry().empty()) {
				ImGui::Separator();
			}

			for (const auto& [
				scene_type,
				registration
			] : impl::GetSceneRegistry()) {
				(void)scene_type;

				if (!ImGui::MenuItem(
						registration
							.display_name
							.c_str()
					)) {
					continue;
				}

				ctx.editor.CreateProjectScene(
					registration.display_name,
					SerializedScene{
						.type =
							registration.type,
						.parameters =
							registration
								.default_parameters(),
						.assets = {},
						.content =
							std::nullopt,
					}
				);

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

bool SceneListPanel::ResolvePendingSceneSelection(Editor& editor) {
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
		editor.GetSceneManager().GetScenes()
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

	auto* scene{ it->get() };

	pending_scene_selection_.reset();

	SetSelectedScene(editor, scene);

	editor
		.GetSceneHierarchyPanel()
		.SetSelectedEntity({});

	return true;
}

void SceneListPanel::QueueSceneSelection(
	Editor& editor,
	std::string scene_tag,
	bool runtime
) {
	pending_scene_selection_ =
		PendingSceneSelection{
			.tag = std::move(scene_tag),
			.runtime = runtime,
			.earliest_frame =
				ImGui::GetFrameCount() + 1,
		};

	editor
		.GetSceneHierarchyPanel()
		.SetSelectedEntity({});
}

void SceneListPanel::SetSelectedScene(
	Editor& editor,
	Scene* scene
) {
	if (selected_scene_ == scene) {
		return;
	}

	auto* previous_scene{ selected_scene_ };

	bool previous_scene_exists{
		previous_scene &&
		std::ranges::any_of(
			editor.GetSceneManager()
				.GetScenes(),
			[previous_scene](
				const auto& active_scene
			) {
				return
					active_scene.get() ==
					previous_scene;
			}
		)
	};

	selected_scene_ = scene;
	state_.reset();

	if (selected_scene_) {
		std::string registered_type{
			selected_scene_->
				GetRegisteredType()
		};

		if (registered_type.empty()) {
			state_ = SceneEditorState{
				.scene_type = {},
				.display_name = "Scene",
				.scene_tag =
					selected_scene_->GetTag(),
				.params = json::object(),
			};
		} else {
			const auto& registration{
				impl::GetSceneRegistration(
					registered_type
				)
			};

			state_ = SceneEditorState{
				.scene_type =
					registration.type,
				.display_name =
					registration.display_name,
				.scene_tag =
					selected_scene_->GetTag(),
				.params =
					registration
						.serialize_parameters(
							*selected_scene_
						),
			};
		}
	}

	editor.OnSelectedSceneChanged(
		previous_scene_exists
			? previous_scene
			: nullptr,
		selected_scene_
	);
}

} // namespace ptgn::editor
