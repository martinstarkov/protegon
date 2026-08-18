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
#include "editor/editor.h"
#include "editor/editor_context.h"
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
		::ptgn::impl::GetSceneRegistration(
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
	std::optional<UUID> selected_entity_uuid = std::nullopt
) {
	auto& hierarchy{
		ctx.editor.GetSceneHierarchyPanel()
	};
	const auto entities{
		scene.Entities()
	};

	if (!selected_entity_uuid &&
		ctx.local.selection.HasEntitySelection(scene.GetTag(), scene.IsRuntime())) {
		selected_entity_uuid =
			ctx.local.selection.GetEntityUUID(scene.GetTag(), scene.IsRuntime());

		if (!selected_entity_uuid) {
			hierarchy.SetSelectedEntity({}, false);
			return;
		}
	}

	auto select_if_present =
		[&](Entity entity) {
			if (!entity ||
				!entities.Contains(entity)) {
				return false;
			}

			hierarchy.SetSelectedEntity(entity, false);
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
		::ptgn::impl::SceneContextAccessor::GetFixedCamera(
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
			::ptgn::impl::kBaseSceneType
		);
		ImGui::CloseCurrentPopup();
	}

	std::vector<
		const ::ptgn::impl::SceneRegistryEntry*
	> registrations;
	registrations.reserve(
		::ptgn::impl::GetSceneRegistry().size()
	);

	for (const auto& [_type, registration] :
		 ::ptgn::impl::GetSceneRegistry()) {
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
	auto* selected_scene{ GetSelectedScene() };
	if (selected_scene || !ctx.local.selection.HasSceneSelection()) {
		return;
	}

	EditorSelection selection{ ctx.local.selection };
	selection.selected_scene_key.clear();
	selection.selected_scene_runtime = false;
	ApplyEditorSelection(ctx, std::move(selection));

	state_.reset();
	key_error_.clear();
}

void SceneListPanel::DrawSceneDetails(
	EditorContext& ctx
) {
	if (pending_scene_selection_) {
		ImGui::SeparatorText("Scene");
		ImGui::TextDisabled("Loading scene...");
		return;
	}

	auto* selected_scene{ GetSelectedScene() };

	if (!selected_scene ||
		!state_) {
		return;
	}

	auto& state{ state_.value() };
	const bool runtime{
		selected_scene->IsRuntime()
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
			selected_scene->GetTag()
		};

		const bool key_submitted{
			ImGui::InputText(
				"Key",
				&state.scene_key,
				ImGuiInputTextFlags_CallbackCharFilter |
					ImGuiInputTextFlags_EnterReturnsTrue,
				&FilterSceneKey
			)
		};

		if (key_submitted ||
			ImGui::IsItemDeactivatedAfterEdit()) {
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
			::ptgn::impl::kBaseSceneType) {
		const auto& registration{
			::ptgn::impl::GetSceneRegistration(
				state.scene_type
			)
		};

		registration.deserialize_parameters(
			state.parameters,
			*selected_scene
		);
		selected_scene->Refresh();
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
		scene,
		false
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
	const bool visible{ ImGui::Begin("Scenes") };

	if (visible) {
		SyncVisibleSceneListTab(ctx, SceneListTab::Scenes);
	}

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

	if (!GetSelectedScene() &&
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
					it->get(),
					false
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
					scene,
					false
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
					scene == GetSelectedScene()
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

				if (!editing_display_name_scene_key_ ||
					*editing_display_name_scene_key_ != entry.key) {
					editing_display_name_scene_key_ = entry.key;
					display_name_edit_buffer_ = entry.display_name;
				}

				const bool display_name_submitted{
					ImGui::InputText(
						"Display Name",
						&display_name_edit_buffer_,
						ImGuiInputTextFlags_EnterReturnsTrue
					)
				};

				if (display_name_submitted ||
					ImGui::IsItemDeactivatedAfterEdit()) {
					if (!ctx.editor.RenameProjectSceneDisplayName(
							entry.key,
							display_name_edit_buffer_
						)) {
						display_name_edit_buffer_ = entry.display_name;
					}

					editing_display_name_scene_key_.reset();
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
						GetSelectedScene()
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

void SceneListPanel::Bind(EditorContext& ctx) {
	context_ = &ctx;
}

Scene* SceneListPanel::GetSelectedScene() const {
	return context_ ? ResolveSelectedScene(*context_) : nullptr;
}

void SceneListPanel::QueueSceneSelection(
	EditorContext& ctx,
	std::string scene_key,
	bool runtime,
	std::optional<UUID> selected_entity_uuid
) {
	ClearInvalidSceneSelection(ctx);

	pending_scene_selection_ = PendingSceneSelection{
		.key = std::move(scene_key),
		.runtime = runtime,
		.selected_entity_uuid = selected_entity_uuid,
		.earliest_frame = ImGui::GetFrameCount() + 1,
	};
}

void SceneListPanel::SetSelectedScene(
	EditorContext& ctx,
	Scene* scene,
	bool undoable
) {
	const auto& loaded_scenes{
		ctx.editor.GetSceneManager().GetScenes()
	};

	const auto is_loaded_scene = [&loaded_scenes](const Scene* candidate) {
		return candidate && std::ranges::any_of(
			loaded_scenes,
			[candidate](const auto& loaded_scene) {
				return loaded_scene && loaded_scene.get() == candidate;
			}
		);
	};

	Scene* next_scene{ is_loaded_scene(scene) ? scene : nullptr };
	Scene* previous_scene{ GetSelectedScene() };
	if (previous_scene == next_scene &&
		(ctx.local.selection.HasSceneSelection() == (next_scene != nullptr))) {
		return;
	}

	EditorSelection selection{ ctx.local.selection };
	if (next_scene) {
		selection.selected_scene_key = next_scene->GetTag();
		selection.selected_scene_runtime = next_scene->IsRuntime();
		selection.mode = EditorSelectionMode::SceneHierarchy;

		if (!selection.HasEntitySelection(next_scene->GetTag(), next_scene->IsRuntime())) {
			const auto entities{ next_scene->Entities() };
			auto& scene_ctx{ next_scene->ctx() };
			const Entity render_target{ next_scene->GetRenderTarget() };
			const Entity fixed_camera{
				::ptgn::impl::SceneContextAccessor::GetFixedCamera(scene_ctx)
			};
			const Entity camera{ scene_ctx.camera };
			Entity selected{
				entities.FindIf([&](Entity entity) {
					return entity != render_target &&
						entity != fixed_camera &&
						entity != camera;
				})
			};

			if (!selected) {
				selected = camera ? camera : fixed_camera;
			}
			if (!selected) {
				selected = render_target;
			}

			selection.SetEntityUUID(
				next_scene->GetTag(),
				next_scene->IsRuntime(),
				selected ? std::optional<UUID>{ selected.Get<UUID>() } : std::nullopt
			);
		}
	} else {
		selection.selected_scene_key.clear();
		selection.selected_scene_runtime = false;
	}

	if (undoable) {
		SetEditorSelection(ctx, std::move(selection), "Select Scene");
	} else {
		ApplyEditorSelection(ctx, std::move(selection));
	}

	RebuildSceneEditorState(next_scene);
}

void SceneListPanel::RefreshSelectedSceneState() {
	RebuildSceneEditorState(GetSelectedScene());
}

void SceneListPanel::RebuildSceneEditorState(Scene* scene) {
	state_.reset();
	key_error_.clear();

	if (!scene) {
		return;
	}

	const std::string scene_type{
		scene->GetRegisteredType().empty()
			? std::string{ ::ptgn::impl::kBaseSceneType }
			: std::string{ scene->GetRegisteredType() }
	};

	if (scene_type == ::ptgn::impl::kBaseSceneType) {
		state_ = SceneEditorState{
			.scene_type = std::string{ ::ptgn::impl::kBaseSceneType },
			.type_display_name = "Scene",
			.scene_key = scene->GetTag(),
			.parameters = json::object(),
		};
		return;
	}

	const auto& registration{ ::ptgn::impl::GetSceneRegistration(scene_type) };
	state_ = SceneEditorState{
		.scene_type = registration.type,
		.type_display_name = TypeNameWithoutNamespaces(registration.type),
		.scene_key = scene->GetTag(),
		.parameters = registration.serialize_parameters(*scene),
	};
}

} // namespace ptgn::editor
