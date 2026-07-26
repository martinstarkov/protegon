#include "panels/scene_list.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/project.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/util/hash.h"
#include "panels/scene_hierarchy.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"

namespace ptgn::editor {

namespace {

struct SceneOrderDrag {
	std::size_t index{ 0 };
	bool runtime{ false };
};

struct PendingSceneOrderMove {
	std::size_t from{ 0 };
	std::size_t to{ 0 };
	bool runtime{ false };
};

[[nodiscard]] bool IsRuntimeSceneList(
	const Editor& editor
) {
	return editor.IsPlaying() ||
		   editor.IsDirectRuntime();
}

[[nodiscard]] Scene* FindLoadedScene(
	Editor& editor,
	std::string_view key,
	bool runtime
) {
	auto& scenes{
		editor.GetSceneManager()
			.GetScenes()
	};

	const auto it{
		std::ranges::find_if(
			scenes,
			[key, runtime](const auto& scene) {
				return scene &&
					   scene->GetTag() == key &&
					   scene->IsRuntime() == runtime;
			}
		)
	};

	return it == scenes.end()
		? nullptr
		: it->get();
}

[[nodiscard]] std::string TypeNameWithoutNamespaces(
	std::string_view type
) {
	const auto separator{ type.rfind("::") };

	return separator ==
			std::string_view::npos
		? std::string{ type }
		: std::string{
			type.substr(separator + 2)
		};
}

[[nodiscard]] std::string SceneTypeDisplayName(
	const Scene& scene
) {
	if (scene.GetRegisteredType().empty()) {
		return "Scene";
	}

	return std::string{
		impl::GetSceneRegistration(
			scene.GetRegisteredType()
		).display_name
	};
}

[[nodiscard]] std::string ProjectSceneLabel(
	const ProjectSceneEntry& entry
) {
	return entry.display_name +
		   " [" + entry.key + "]";
}

int FilterSceneKey(
	ImGuiInputTextCallbackData* data
) {
	const auto character{
		static_cast<unsigned char>(
			data->EventChar
		)
	};

	return std::isalnum(character) ||
		   character == '_' ||
		   character == '-'
		? 0
		: 1;
}

bool DrawJsonEditor(
	const char* label,
	json& value
) {
	bool changed{ false };

	if (value.is_boolean()) {
		bool current{ value.get<bool>() };

		if (ImGui::Checkbox(label, &current)) {
			value = current;
			changed = true;
		}
	} else if (value.is_number_integer()) {
		int current{ value.get<int>() };

		if (ImGui::InputInt(label, &current)) {
			value = current;
			changed = true;
		}
	} else if (value.is_number_unsigned()) {
		std::uint64_t current{
			value.get<std::uint64_t>()
		};

		if (ImGui::InputScalar(
				label,
				ImGuiDataType_U64,
				&current
			)) {
			value = current;
			changed = true;
		}
	} else if (value.is_number_float()) {
		float current{ value.get<float>() };

		if (ImGui::InputFloat(label, &current)) {
			value = current;
			changed = true;
		}
	} else if (value.is_string()) {
		std::string current{
			value.get<std::string>()
		};

		if (ImGui::InputText(label, &current)) {
			value = std::move(current);
			changed = true;
		}
	} else if (value.is_object()) {
		if (ImGui::TreeNode(label)) {
			for (auto it{ value.begin() };
				 it != value.end();
				 ++it) {
				ImGui::PushID(it.key().c_str());
				changed |= DrawJsonEditor(
					it.key().c_str(),
					it.value()
				);
				ImGui::PopID();
			}

			ImGui::TreePop();
		}
	} else if (value.is_array()) {
		if (ImGui::TreeNode(label)) {
			for (std::size_t index{ 0 };
				 index < value.size();
				 ++index) {
				const std::string item{
					"[" + std::to_string(index) + "]"
				};

				ImGui::PushID(
					static_cast<int>(index)
				);
				changed |= DrawJsonEditor(
					item.c_str(),
					value[index]
				);
				ImGui::PopID();
			}

			ImGui::TreePop();
		}
	} else {
		ImGui::TextDisabled(
			"%s: unsupported",
			label
		);
	}

	return changed;
}

void SelectDefaultEntity(
	EditorContext& ctx,
	Scene& scene,
	std::optional<UUID> selected_entity_uuid =
		std::nullopt
) {
	auto& hierarchy{
		ctx.editor.GetSceneHierarchyPanel()
	};
	const auto entities{
		scene.Entities()
	};

	auto select_if_present =
		[&](Entity entity) {
			if (!entity ||
				!entities.Contains(entity)) {
				return false;
			}

			hierarchy.SetSelectedEntity(entity);
			return true;
		};

	if (selected_entity_uuid &&
		select_if_present(
			scene.GetEntity(
				selected_entity_uuid.value()
			)
		)) {
		return;
	}

	auto& scene_ctx{ scene.ctx() };
	const auto render_target{
		scene.GetRenderTarget()
	};
	const auto fixed_camera{
		impl::SceneContextAccessor::GetFixedCamera(
			scene_ctx
		)
	};
	const auto camera{ scene_ctx.camera };

	const auto regular_entity{
		entities.FindIf(
			[&](Entity entity) {
				return entity != render_target &&
					   entity != fixed_camera &&
					   entity != camera;
			}
		)
	};

	if (select_if_present(regular_entity) ||
		select_if_present(camera) ||
		select_if_present(fixed_camera)) {
		return;
	}

	select_if_present(render_target);
}

void DrawSceneOrderDragSource(
	std::size_t index,
	bool runtime,
	std::string_view label
) {
	if (!ImGui::BeginDragDropSource()) {
		return;
	}

	const SceneOrderDrag payload{
		.index = index,
		.runtime = runtime,
	};

	ImGui::SetDragDropPayload(
		"PTGN_SCENE_ORDER",
		&payload,
		sizeof(payload)
	);

	ImGui::TextUnformatted(
		label.data(),
		label.data() + label.size()
	);

	ImGui::EndDragDropSource();
}

void DrawSceneOrderDropTarget(
	std::size_t index,
	bool runtime,
	std::optional<PendingSceneOrderMove>& pending_move
) {
	if (!ImGui::BeginDragDropTarget()) {
		return;
	}

	if (const auto* payload{
			ImGui::AcceptDragDropPayload(
				"PTGN_SCENE_ORDER"
			)
		}) {
		PTGN_ASSERT(
			payload->DataSize ==
				sizeof(SceneOrderDrag)
		);

		const auto dragged{
			*static_cast<const SceneOrderDrag*>(
				payload->Data
			)
		};

		if (dragged.runtime == runtime &&
			dragged.index != index) {
			pending_move =
				PendingSceneOrderMove{
					.from = dragged.index,
					.to = index,
					.runtime = runtime,
				};
		}
	}

	ImGui::EndDragDropTarget();
}

void MoveRuntimeScene(
	Editor& editor,
	std::size_t from_index,
	std::size_t to_index
) {
	std::vector<std::string> keys;

	for (const auto& scene :
		 editor.GetSceneManager().GetScenes()) {
		if (scene &&
			scene->IsRuntime()) {
			keys.emplace_back(
				scene->GetTag()
			);
		}
	}

	if (from_index >= keys.size() ||
		to_index >= keys.size() ||
		from_index == to_index) {
		return;
	}

	if (from_index < to_index) {
		std::rotate(
			keys.begin() +
				static_cast<std::ptrdiff_t>(from_index),
			keys.begin() +
				static_cast<std::ptrdiff_t>(from_index + 1),
			keys.begin() +
				static_cast<std::ptrdiff_t>(to_index + 1)
		);
	} else {
		std::rotate(
			keys.begin() +
				static_cast<std::ptrdiff_t>(to_index),
			keys.begin() +
				static_cast<std::ptrdiff_t>(from_index),
			keys.begin() +
				static_cast<std::ptrdiff_t>(from_index + 1)
		);
	}

	editor.GetSceneManager().ReorderScenes(
		keys,
		true
	);
}

void DrawAddScenePopup(
	EditorContext& ctx
) {
	if (!ImGui::BeginPopup("AddProjectScene")) {
		return;
	}

	if (ImGui::MenuItem("Scene")) {
		ctx.editor.CreateProjectScene(
			impl::kBaseSceneType
		);
		ImGui::CloseCurrentPopup();
	}

	std::vector<
		const impl::SceneRegistryEntry*
	> registrations;
	registrations.reserve(
		impl::GetSceneRegistry().size()
	);

	for (const auto& [type, registration] :
		 impl::GetSceneRegistry()) {
		(void)type;
		registrations.emplace_back(
			&registration
		);
	}

	std::ranges::sort(
		registrations,
		{},
		[](const auto* registration) {
			return registration->display_name;
		}
	);

	if (!registrations.empty()) {
		ImGui::Separator();
	}

	for (const auto* registration :
		 registrations) {
		PTGN_ASSERT(registration);

		if (ImGui::MenuItem(
				registration->display_name.c_str()
			)) {
			ctx.editor.CreateProjectScene(
				registration->type
			);
			ImGui::CloseCurrentPopup();
			break;
		}
	}

	ImGui::EndPopup();
}

} // namespace

void SceneListPanel::ClearInvalidSceneSelection(
	EditorContext& ctx
) {
	if (!selected_scene_) {
		return;
	}

	const bool runtime_scene_list{
		ctx.editor.IsPlaying()
	};
	const auto& loaded_scenes{
		ctx.editor
			.GetSceneManager()
			.GetScenes()
	};

	const bool selected_scene_exists{
		std::ranges::any_of(
			loaded_scenes,
			[this, runtime_scene_list](
				const auto& scene
			) {
				return scene &&
					   scene.get() == selected_scene_ &&
					   scene->IsRuntime() ==
						   runtime_scene_list;
			}
		)
	};

	if (selected_scene_exists) {
		return;
	}

	// The SceneManager has already destroyed this scene. Do not pass the
	// old pointer to OnSelectedSceneChanged or dereference it anywhere.
	selected_scene_ = nullptr;
	state_.reset();
	key_error_.clear();

	ctx.editor
		.GetSceneHierarchyPanel()
		.SetSelectedEntity({});
}

void SceneListPanel::DrawSceneDetails(
	EditorContext& ctx
) {
	if (pending_scene_selection_) {
		ImGui::SeparatorText("Scene");
		ImGui::TextDisabled("Loading scene...");
		return;
	}

	if (!selected_scene_ ||
		!state_) {
		return;
	}

	auto& state{ state_.value() };
	const bool runtime{
		selected_scene_->IsRuntime()
	};

	ImGui::SeparatorText("Scene");
	ImGui::Text(
		"Type: %s",
		state.type_display_name.c_str()
	);

	if (runtime) {
		ImGui::InputText(
			"Key",
			&state.scene_key,
			ImGuiInputTextFlags_ReadOnly
		);
	} else {
		const std::string previous_key{
			selected_scene_->GetTag()
		};

		ImGui::InputText(
			"Key",
			&state.scene_key,
			ImGuiInputTextFlags_CallbackCharFilter,
			&FilterSceneKey
		);

		if (ImGui::IsItemDeactivatedAfterEdit()) {
			if (ctx.editor.RenameProjectSceneKey(
					previous_key,
					state.scene_key
				)) {
				key_error_.clear();
			} else {
				state.scene_key = previous_key;
				key_error_ =
					"Scene keys must be non-empty and unique.";
			}
		}

		if (!key_error_.empty()) {
			ImGui::TextDisabled(
				"%s",
				key_error_.c_str()
			);
		}
	}

	if (state.parameters.empty()) {
		ImGui::TextDisabled(
			"No serialized scene parameters."
		);
		return;
	}

	bool changed{ false };

	for (auto it{ state.parameters.begin() };
		 it != state.parameters.end();
		 ++it) {
		ImGui::PushID(it.key().c_str());
		changed |= DrawJsonEditor(
			it.key().c_str(),
			it.value()
		);
		ImGui::PopID();
	}

	if (!changed) {
		return;
	}

	if (state.scene_type !=
			impl::kBaseSceneType) {
		const auto& registration{
			impl::GetSceneRegistration(
				state.scene_type
			)
		};

		registration.deserialize_parameters(
			state.parameters,
			*selected_scene_
		);
		selected_scene_->Refresh();
	}

	if (!runtime) {
		ctx.editor.MarkProjectDirty();
	}
}

bool SceneListPanel::ResolvePendingSceneSelection(
	EditorContext& ctx
) {
	if (!pending_scene_selection_ ||
		ImGui::GetFrameCount() <
			pending_scene_selection_
				->earliest_frame) {
		return false;
	}

	const auto pending{
		pending_scene_selection_.value()
	};

	auto* scene{
		FindLoadedScene(
			ctx.editor,
			pending.key,
			pending.runtime
		)
	};

	if (!scene) {
		return false;
	}

	pending_scene_selection_.reset();

	SetSelectedScene(
		ctx,
		scene
	);

	SelectDefaultEntity(
		ctx,
		*scene,
		pending.selected_entity_uuid
	);

	return true;
}

void SceneListPanel::OnRender(
	EditorContext& ctx
) {
	ImGui::Begin("Scenes");

	ClearInvalidSceneSelection(ctx);

	const bool runtime_scene_list{
		IsRuntimeSceneList(ctx.editor)
	};
	auto& manager{
		ctx.editor.GetSceneManager()
	};
	auto& loaded_scenes{
		manager.GetScenes()
	};
	auto* project{
		ctx.editor.GetProject()
	};

	if (!selected_scene_ &&
		!pending_scene_selection_) {
		if (runtime_scene_list) {
			const auto it{
				std::ranges::find_if(
					loaded_scenes,
					[](const auto& scene) {
						return scene &&
							   scene->IsRuntime();
					}
				)
			};

			if (it != loaded_scenes.end()) {
				SetSelectedScene(
					ctx,
					it->get()
				);
				SelectDefaultEntity(
					ctx,
					**it
				);
			}
		} else if (project &&
				   !project->scenes.empty()) {
			if (auto* scene{
					FindLoadedScene(
						ctx.editor,
						project->scenes.front().key,
						false
					)
				}) {
				SetSelectedScene(
					ctx,
					scene
				);
				SelectDefaultEntity(
					ctx,
					*scene
				);
			}
		}
	}

	std::optional<PendingSceneOrderMove>
		pending_move;

	if (!runtime_scene_list) {
		ImGui::BeginDisabled(!project);

		if (ImGui::Button(
				"+ Add Scene",
				ImVec2{
					-1.0f,
					0.0f
				}
			)) {
			ImGui::OpenPopup(
				"AddProjectScene"
			);
		}

		ImGui::EndDisabled();
		DrawAddScenePopup(ctx);

		ImGui::SeparatorText(
			"Project Scenes"
		);

		if (!project) {
			ImGui::TextDisabled(
				"No project is open."
			);
		} else {
			for (std::size_t index{ 0 };
				 index < project->scenes.size();
				 ++index) {
				auto& entry{
					project->scenes[index]
				};
				auto* scene{
					FindLoadedScene(
						ctx.editor,
						entry.key,
						false
					)
				};

				const bool selected{
					scene &&
					scene == selected_scene_
				};
				std::string label{
					ProjectSceneLabel(entry)
				};

				if (ctx.editor
						.IsStartupProjectScene(
							entry.key
						)) {
					label += " [Startup]";
				}

				const std::string item_label{
					label +
					"###ProjectScene" +
					std::to_string(index)
				};

				if (ImGui::Selectable(
						item_label.c_str(),
						selected
					) &&
					scene) {
					SetSelectedScene(
						ctx,
						scene
					);
					SelectDefaultEntity(
						ctx,
						*scene
					);
				}

				DrawSceneOrderDragSource(
					index,
					false,
					label
				);
				DrawSceneOrderDropTarget(
					index,
					false,
					pending_move
				);

				if (!ImGui::BeginPopupContextItem()) {
					continue;
				}

				std::string display_name{
					entry.display_name
				};

				if (ImGui::InputText(
						"Display Name",
						&display_name
					)) {
					ctx.editor
						.RenameProjectSceneDisplayName(
							entry.key,
							std::move(display_name)
						);
				}

				if (ImGui::MenuItem(
						"Duplicate"
					)) {
					ctx.editor
						.DuplicateProjectScene(
							entry.key
						);
					ImGui::EndPopup();
					break;
				}

				const bool startup{
					ctx.editor
						.IsStartupProjectScene(
							entry.key
						)
				};

				if (ImGui::MenuItem(
						"Set as Startup Scene",
						nullptr,
						startup,
						!startup
					)) {
					ctx.editor
						.SetStartupProjectScene(
							entry.key
						);
				}

				ImGui::Separator();

				const bool can_delete{
					project->scenes.size() > 1
				};

				if (ImGui::MenuItem(
						"Delete",
						nullptr,
						false,
						can_delete
					)) {
					ctx.editor
						.DeleteProjectScene(
							entry.key
						);
					ImGui::EndPopup();
					break;
				}

				ImGui::EndPopup();
			}
		}
	} else {
		ImGui::SeparatorText(
			"Active Runtime Scenes"
		);

		std::size_t runtime_index{ 0 };

		for (const auto& scene :
			 loaded_scenes) {
			if (!scene ||
				!scene->IsRuntime()) {
				continue;
			}

			const auto* entry{
				project
					? FindProjectScene(
						*project,
						scene->GetTag()
					)
					: nullptr
			};

			std::string label{
				entry
					? ProjectSceneLabel(*entry)
					: SceneTypeDisplayName(*scene) +
					  " [" + scene->GetTag() + "]"
			};

			if (scene->IsTransitioning()) {
				label += " [Transitioning]";
			}

			const std::string item_label{
				label +
				"###RuntimeScene" +
				std::to_string(
					runtime_index
				)
			};

			if (ImGui::Selectable(
					item_label.c_str(),
					scene.get() ==
						selected_scene_
				)) {
				SetSelectedScene(
					ctx,
					scene.get()
				);
				SelectDefaultEntity(
					ctx,
					*scene
				);
			}

			DrawSceneOrderDragSource(
				runtime_index,
				true,
				label
			);
			DrawSceneOrderDropTarget(
				runtime_index,
				true,
				pending_move
			);

			++runtime_index;
		}
	}

	if (pending_move) {
		if (pending_move->runtime) {
			MoveRuntimeScene(
				ctx.editor,
				pending_move->from,
				pending_move->to
			);
		} else {
			ctx.editor.MoveProjectScene(
				pending_move->from,
				pending_move->to
			);
		}
	}

	DrawSceneDetails(ctx);

	ImGui::End();
}

Scene* SceneListPanel::GetSelectedScene() const {
	return selected_scene_;
}
void SceneListPanel::QueueSceneSelection(
	EditorContext& ctx,
	std::string scene_key,
	bool runtime,
	std::optional<UUID> selected_entity_uuid
) {
	// A transition may already have destroyed the selected scene before
	// this request is processed.
	ClearInvalidSceneSelection(ctx);

	pending_scene_selection_ =
		PendingSceneSelection{
			.key =
				std::move(scene_key),
			.runtime =
				runtime,
			.selected_entity_uuid =
				std::move(
					selected_entity_uuid
				),
			.earliest_frame =
				ImGui::GetFrameCount() + 1,
		};

	// The current scene is still alive here if it survived validation,
	// so this safely disables its entity-picking framebuffer state.
	SetSelectedScene(
		ctx,
		nullptr
	);

	ctx.editor
		.GetSceneHierarchyPanel()
		.SetSelectedEntity({});
}

void SceneListPanel::SetSelectedScene(
	EditorContext& ctx,
	Scene* scene
) {
	const auto& loaded_scenes{
		ctx.editor
			.GetSceneManager()
			.GetScenes()
	};

	const auto is_loaded_scene{
		[&loaded_scenes](
			const Scene* candidate
		) {
			if (!candidate) {
				return false;
			}

			return std::ranges::any_of(
				loaded_scenes,
				[candidate](
					const auto& loaded_scene
				) {
					return loaded_scene &&
						   loaded_scene.get() ==
							   candidate;
				}
			);
		}
	};

	// Never pass an already-destroyed previous scene to Editor.
	auto* previous_scene{
		is_loaded_scene(selected_scene_)
			? selected_scene_
			: nullptr
	};

	// Never install or dereference a pointer that is no longer owned by
	// SceneManager.
	auto* next_scene{
		is_loaded_scene(scene)
			? scene
			: nullptr
	};

	// Compare against the stored pointer rather than previous_scene.
	// If selected_scene_ is dangling and next_scene is null, the stale
	// selection still needs to be cleared.
	if (selected_scene_ == next_scene) {
		return;
	}

	selected_scene_ = next_scene;
	state_.reset();
	key_error_.clear();

	if (selected_scene_) {
		const std::string scene_type{
			selected_scene_
				->GetRegisteredType()
				.empty()
				? std::string{
					impl::kBaseSceneType
				}
				: std::string{
					selected_scene_
						->GetRegisteredType()
				}
		};

		if (scene_type ==
			impl::kBaseSceneType) {
			state_ =
				SceneEditorState{
					.scene_type =
						std::string{
							impl::kBaseSceneType
						},
					.type_display_name =
						"Scene",
					.scene_key =
						selected_scene_
							->GetTag(),
					.parameters =
						json::object(),
				};
		} else {
			const auto& registration{
				impl::GetSceneRegistration(
					scene_type
				)
			};

			state_ =
				SceneEditorState{
					.scene_type =
						registration.type,
					.type_display_name =
						TypeNameWithoutNamespaces(
							registration.type
						),
					.scene_key =
						selected_scene_
							->GetTag(),
					.parameters =
						registration
							.serialize_parameters(
								*selected_scene_
							),
				};
		}
	}

	ctx.editor.OnSelectedSceneChanged(
		previous_scene,
		selected_scene_
	);
}

} // namespace ptgn::editor
