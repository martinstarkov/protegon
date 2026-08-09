#include "core/editor.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <vector>

#include "app/application.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/assert.h"
#include "core/editor_context.h"
#include "core/editor_selection.h"
#include "core/editor_state.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "panels/content_browser.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/settings.h"
#include "panels/viewport.h"
#include "platform/window.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/uuid.h"
#include "app/project.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "serialization/json/json.h"
#include "tools/debug/debug_system.h"

namespace ptgn::editor {

namespace {

constexpr float kLeftColumnRatio{ 0.25f };
constexpr float kRightColumnRatio{ 0.40f };

class ScopedRuntimeEditorTheme {
public:
	explicit ScopedRuntimeEditorTheme(
		bool enabled
	) {
		if (!enabled) {
			return;
		}

		// Main panel and popup backgrounds.
		Push(
			ImGuiCol_WindowBg,
			ImVec4{ 0.105f, 0.095f, 0.055f, 1.0f }
		);
		Push(
			ImGuiCol_ChildBg,
			ImVec4{ 0.095f, 0.085f, 0.050f, 1.0f }
		);
		Push(
			ImGuiCol_PopupBg,
			ImVec4{ 0.130f, 0.115f, 0.060f, 1.0f }
		);
		Push(
			ImGuiCol_Border,
			ImVec4{ 0.340f, 0.280f, 0.100f, 0.75f }
		);

		// Inputs, checkboxes, sliders, and drag controls.
		Push(
			ImGuiCol_FrameBg,
			ImVec4{ 0.230f, 0.190f, 0.065f, 1.0f }
		);
		Push(
			ImGuiCol_FrameBgHovered,
			ImVec4{ 0.360f, 0.290f, 0.080f, 1.0f }
		);
		Push(
			ImGuiCol_FrameBgActive,
			ImVec4{ 0.470f, 0.370f, 0.090f, 1.0f }
		);

		Push(
			ImGuiCol_CheckMark,
			ImVec4{ 0.820f, 0.670f, 0.200f, 1.0f }
		);
		Push(
			ImGuiCol_CheckboxSelectedBg,
			ImVec4{ 0.460f, 0.360f, 0.080f, 1.0f }
		);

		Push(
			ImGuiCol_SliderGrab,
			ImVec4{ 0.640f, 0.510f, 0.130f, 1.0f }
		);
		Push(
			ImGuiCol_SliderGrabActive,
			ImVec4{ 0.820f, 0.650f, 0.160f, 1.0f }
		);

		// Buttons, tree nodes, selectables, and collapsing headers.
		Push(
			ImGuiCol_Button,
			ImVec4{ 0.280f, 0.230f, 0.070f, 1.0f }
		);
		Push(
			ImGuiCol_ButtonHovered,
			ImVec4{ 0.420f, 0.340f, 0.090f, 1.0f }
		);
		Push(
			ImGuiCol_ButtonActive,
			ImVec4{ 0.540f, 0.430f, 0.110f, 1.0f }
		);

		Push(
			ImGuiCol_Header,
			ImVec4{ 0.260f, 0.215f, 0.065f, 1.0f }
		);
		Push(
			ImGuiCol_HeaderHovered,
			ImVec4{ 0.410f, 0.330f, 0.090f, 1.0f }
		);
		Push(
			ImGuiCol_HeaderActive,
			ImVec4{ 0.520f, 0.410f, 0.105f, 1.0f }
		);

		// Title bars and menu bars.
		Push(
			ImGuiCol_TitleBg,
			ImVec4{ 0.125f, 0.110f, 0.055f, 1.0f }
		);
		Push(
			ImGuiCol_TitleBgActive,
			ImVec4{ 0.235f, 0.195f, 0.065f, 1.0f }
		);
		Push(
			ImGuiCol_TitleBgCollapsed,
			ImVec4{ 0.100f, 0.090f, 0.050f, 1.0f }
		);
		Push(
			ImGuiCol_MenuBarBg,
			ImVec4{ 0.150f, 0.130f, 0.055f, 1.0f }
		);

		// Scrollbars.
		Push(
			ImGuiCol_ScrollbarBg,
			ImVec4{ 0.080f, 0.075f, 0.040f, 1.0f }
		);
		Push(
			ImGuiCol_ScrollbarGrab,
			ImVec4{ 0.290f, 0.240f, 0.075f, 1.0f }
		);
		Push(
			ImGuiCol_ScrollbarGrabHovered,
			ImVec4{ 0.400f, 0.320f, 0.090f, 1.0f }
		);
		Push(
			ImGuiCol_ScrollbarGrabActive,
			ImVec4{ 0.500f, 0.390f, 0.100f, 1.0f }
		);

		// Separators and resize handles.
		Push(
			ImGuiCol_Separator,
			ImVec4{ 0.310f, 0.260f, 0.085f, 1.0f }
		);
		Push(
			ImGuiCol_SeparatorHovered,
			ImVec4{ 0.560f, 0.450f, 0.120f, 1.0f }
		);
		Push(
			ImGuiCol_SeparatorActive,
			ImVec4{ 0.720f, 0.570f, 0.145f, 1.0f }
		);

		Push(
			ImGuiCol_ResizeGrip,
			ImVec4{ 0.380f, 0.310f, 0.085f, 0.35f }
		);
		Push(
			ImGuiCol_ResizeGripHovered,
			ImVec4{ 0.600f, 0.480f, 0.125f, 0.75f }
		);
		Push(
			ImGuiCol_ResizeGripActive,
			ImVec4{ 0.760f, 0.600f, 0.150f, 1.0f }
		);

		// Docked panel tabs.
		Push(
			ImGuiCol_Tab,
			ImVec4{ 0.150f, 0.130f, 0.055f, 1.0f }
		);
		Push(
			ImGuiCol_TabHovered,
			ImVec4{ 0.420f, 0.335f, 0.090f, 1.0f }
		);
		Push(
			ImGuiCol_TabSelected,
			ImVec4{ 0.300f, 0.250f, 0.070f, 1.0f }
		);
		Push(
			ImGuiCol_TabSelectedOverline,
			ImVec4{ 0.720f, 0.570f, 0.140f, 1.0f }
		);
		Push(
			ImGuiCol_TabDimmed,
			ImVec4{ 0.110f, 0.100f, 0.050f, 1.0f }
		);
		Push(
			ImGuiCol_TabDimmedSelected,
			ImVec4{ 0.220f, 0.185f, 0.060f, 1.0f }
		);
		Push(
			ImGuiCol_TabDimmedSelectedOverline,
			ImVec4{ 0.500f, 0.400f, 0.100f, 1.0f }
		);

		// Docking regions.
		Push(
			ImGuiCol_DockingPreview,
			ImVec4{ 0.720f, 0.570f, 0.140f, 0.70f }
		);
		Push(
			ImGuiCol_DockingEmptyBg,
			ImVec4{ 0.080f, 0.075f, 0.040f, 1.0f }
		);

		// Tables used throughout the Inspector.
		Push(
			ImGuiCol_TableHeaderBg,
			ImVec4{ 0.190f, 0.160f, 0.055f, 1.0f }
		);
		Push(
			ImGuiCol_TableBorderStrong,
			ImVec4{ 0.350f, 0.290f, 0.090f, 1.0f }
		);
		Push(
			ImGuiCol_TableBorderLight,
			ImVec4{ 0.240f, 0.200f, 0.065f, 1.0f }
		);

		// Other highlights that otherwise remain blue.
		Push(
			ImGuiCol_TextSelectedBg,
			ImVec4{ 0.500f, 0.400f, 0.100f, 0.45f }
		);
		Push(
			ImGuiCol_TreeLines,
			ImVec4{ 0.460f, 0.370f, 0.110f, 1.0f }
		);
		Push(
			ImGuiCol_DragDropTarget,
			ImVec4{ 0.850f, 0.680f, 0.180f, 1.0f }
		);
		Push(
			ImGuiCol_DragDropTargetBg,
			ImVec4{ 0.650f, 0.510f, 0.110f, 0.20f }
		);
		Push(
			ImGuiCol_NavCursor,
			ImVec4{ 0.780f, 0.620f, 0.160f, 1.0f }
		);
	}

	~ScopedRuntimeEditorTheme() {
		if (color_count_ > 0) {
			ImGui::PopStyleColor(color_count_);
		}
	}

	ScopedRuntimeEditorTheme(
		const ScopedRuntimeEditorTheme&
	) = delete;

	ScopedRuntimeEditorTheme& operator=(
		const ScopedRuntimeEditorTheme&
	) = delete;

private:
	void Push(
		ImGuiCol target,
		ImVec4 color
	) {
		ImGui::PushStyleColor(target, color);
		++color_count_;
	}

	int color_count_{ 0 };
};

[[nodiscard]] std::optional<UUID> GetSelectedEntityUUID(
	const SceneHierarchyPanel& hierarchy,
	const Scene* scene
) {
	if (!scene) {
		return std::nullopt;
	}

	Entity selected_entity{
		hierarchy.GetSelectedEntity()
	};

	if (!selected_entity ||
		std::addressof(selected_entity.GetScene()) != scene) {
		return std::nullopt;
	}

	return selected_entity.Get<UUID>();
}

void SaveProjectManifest(Application& app, Project& project) {
	auto& assets{ impl::ApplicationAccessor::ctx(app).assets };
	assets.RefreshCatalogFromDisk();
	project.assets = assets.GetCatalog();
	project.preload_assets = assets.GetProjectAssetDependencies();
	project.settings = GetProjectSettings(app);
	SaveProject(project);
}

[[nodiscard]] std::string SceneTypeName(
	std::string_view scene_type
) {
	if (scene_type == impl::kBaseSceneType) {
		return "Scene";
	}

	const auto separator{ scene_type.rfind("::") };
	return separator == std::string_view::npos
		? std::string{ scene_type }
		: std::string{ scene_type.substr(separator + 2) };
}

[[nodiscard]] std::string SanitizeSceneKey(
	std::string_view value
) {
	std::string result;
	result.reserve(value.size());

	for (char c : value) {
		const auto byte{ static_cast<unsigned char>(c) };

		if (std::isalnum(byte) ||
			c == '_' ||
			c == '-') {
			result.push_back(c);
		}
	}

	return result.empty()
		? std::string{ "Scene" }
		: result;
}

[[nodiscard]] std::string MakeUniqueProjectSceneKey(
	const Project& project,
	std::string_view preferred_name
) {
	const std::string base{
		SanitizeSceneKey(preferred_name)
	};

	auto is_available =
		[&project](std::string_view candidate) {
			return FindProjectScene(
				project,
				candidate
			) == nullptr;
		};

	if (is_available(base)) {
		return base;
	}

	for (std::size_t index{ 2 };; ++index) {
		std::string candidate{
			base + std::to_string(index)
		};

		if (is_available(candidate)) {
			return candidate;
		}
	}
}

[[nodiscard]] std::string SanitizeSceneFileName(
	std::string_view preferred_name
) {
	std::string result;
	result.reserve(preferred_name.size());

	for (char c : preferred_name) {
		const auto byte{
			static_cast<unsigned char>(c)
		};

		if (std::isalnum(byte) ||
			c == ' ' ||
			c == '-' ||
			c == '_') {
			result.push_back(c);
		} else {
			result.push_back('_');
		}
	}

	while (!result.empty() &&
		   (result.back() == ' ' ||
			result.back() == '.')) {
		result.pop_back();
	}

	return result.empty()
		? std::string{ "Scene" }
		: result;
}

[[nodiscard]] path MakeUniqueProjectScenePath(
	const Project& project,
	std::string_view preferred_name
) {
	const std::string base{
		SanitizeSceneFileName(preferred_name)
	};

	auto is_available =
		[&project](
			const path& candidate
		) {
			const bool used_by_project{
				std::ranges::any_of(
					project.scenes,
					[&candidate](
						const ProjectSceneEntry& scene
					) {
						return
							scene.scene_path.lexically_normal()
								.generic_string() ==
							candidate.lexically_normal()
								.generic_string();
					}
				)
			};

			return !used_by_project &&
				   !FileExists(
					   project.file_path.parent_path() /
					   candidate
				   );
		};

	path candidate{
		project.asset_directory /
		"Scenes" /
		(base + ".ptgnscene")
	};

	if (is_available(candidate)) {
		return candidate;
	}

	for (std::size_t index{ 2 };; ++index) {
		candidate =
			project.asset_directory /
			"Scenes" /
			(
				base + " " +
				std::to_string(index) +
				".ptgnscene"
			);

		if (is_available(candidate)) {
			return candidate;
		}
	}
}

[[nodiscard]] SerializedScene MakeProjectSceneDefinition(
	std::string_view scene_type
) {
	if (scene_type == impl::kBaseSceneType) {
		return SerializedScene{
			.type = std::string{ impl::kBaseSceneType },
			.parameters = json::object(),
			.assets = {},
			.preload_assets = {},
			.content = std::nullopt,
		};
	}

	const auto& registration{
		impl::GetSceneRegistration(scene_type)
	};

	return SerializedScene{
		.type = registration.type,
		.parameters = registration.default_parameters(),
		.assets = {},
		.preload_assets = {},
		.content = std::nullopt,
	};
}


} // namespace

Editor::Editor(Application& app) : app{ app } {
	app.SetCloseGuard([this]() { return content_browser_panel_.CanApplicationClose(); });

	// Generic application startup preference. The engine does not know why it was changed.
	impl::ApplicationAccessor::ctx(app).start_project_runtime = false;

	context_ = std::make_unique<EditorContext>(
		*this, commands_, undo_stack_
	);

	commands_.Bind(*context_);
	scene_hierarchy_panel_.Bind(*context_);
	scene_list_panel_.Bind(*context_);
}

void Editor::RequestQuit() {
	app.RequestQuit();
}

void Editor::UpdateDockLayout(std::uint32_t dockspace_id, float width) {
	auto* dockspace{ ImGui::DockBuilderGetNode(dockspace_id) };

	if (!dockspace) {
		dock_layout_built_ = false;
		BuildDefaultDockLayout(dockspace_id);
		return;
	}

	ImGui::DockBuilderSetNodeSize(
		dockspace_id, ImVec2{
						  ImGui::GetMainViewport()->WorkSize.x,
						  ImGui::GetMainViewport()->WorkSize.y,
					  }
	);

	if (auto* left{ ImGui::DockBuilderGetNode(dock_left_column_id_) }) {
		left->SizeRef.x = width * kLeftColumnRatio;
	}

	if (auto* right{ ImGui::DockBuilderGetNode(dock_right_column_id_) }) {
		right->SizeRef.x = width * kRightColumnRatio;
	}
}

void Editor::OnRender() {
	ScopedRuntimeEditorTheme runtime_theme{
		IsPlaying()
	};

	auto* viewport{ ImGui::GetMainViewport() };

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	auto window_flags{ ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
					   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
					   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar };

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	ImGui::Begin("EditorRootDockspace", nullptr, window_flags);

	ImGui::PopStyleVar(3);

	DrawMainMenuBar();

	auto dockspace_id{ ImGui::GetID("EditorDockspace") };
	auto dockspace_size{ ImGui::GetContentRegionAvail() };

	BuildDefaultDockLayout(dockspace_id);

	if (dock_resize_frames_remaining_ > 0) {
		UpdateDockLayout(dockspace_id, dockspace_size.x);
		--dock_resize_frames_remaining_;
	}

	ImGui::DockSpace(dockspace_id, dockspace_size, ImGuiDockNodeFlags_None);

	DrawPanels();

	ImGui::End();

	SaveEditorLocalStateIfChanged();
}

Project* Editor::GetProject() {
	return impl::ApplicationAccessor::ctx(app)
		.project
		? std::addressof(
			impl::ApplicationAccessor::ctx(app)
				.project.value()
		)
		: nullptr;
}

const Project* Editor::GetProject() const {
	const auto& project{
		impl::ApplicationAccessor::ctx(app)
			.project
	};

	return project
		? std::addressof(project.value())
		: nullptr;
}

void Editor::MarkProjectDirty() {
	PTGN_ASSERT(context_);
	context_->local.state.is_dirty = true;
	scene_asset_dependencies_dirty_ = true;
}

bool Editor::CreateProjectScene(
	std::string_view scene_type
) {
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return false;
	}

	auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	if (scene_type != impl::kBaseSceneType &&
		!impl::GetSceneRegistry().contains(scene_type)) {
		return false;
	}

	const std::string display_name{
		SceneTypeName(scene_type)
	};
	const std::string scene_key{
		MakeUniqueProjectSceneKey(
			*project,
			display_name
		)
	};
	const path relative_path{
		MakeUniqueProjectScenePath(
			*project,
			display_name
		)
	};

	const SerializedScene serialized_scene{
		MakeProjectSceneDefinition(scene_type)
	};
	const ProjectSceneEntry entry{
		.key = scene_key,
		.display_name = display_name,
		.scene_path = relative_path,
	};
	const std::size_t insertion_index{
		project->scenes.size()
	};
	const bool bootstrap_save{
		!serialized_scene.content.has_value()
	};
	const EditorSelection before_selection{
		context_->local.selection
	};
	EditorSelection after_selection{
		before_selection
	};
	after_selection.selected_scene_key = entry.key;
	after_selection.selected_scene_runtime = false;
	after_selection.mode = EditorSelectionMode::SceneHierarchy;

	auto create_scene = [
		this,
		entry,
		serialized_scene,
		insertion_index,
		bootstrap_save
	](EditorSelection selection) mutable {
		auto* current_project{ GetProject() };
		if (!current_project ||
			FindProjectScene(*current_project, entry.key)) {
			return false;
		}

		const path absolute_path{
			GetProjectScenePath(*current_project, entry)
		};
		SaveSceneFile(absolute_path, serialized_scene);

		const std::size_t index{
			std::min(insertion_index, current_project->scenes.size())
		};
		current_project->scenes.insert(
			current_project->scenes.begin() +
				static_cast<std::ptrdiff_t>(index),
			entry
		);

		auto& manager{ GetSceneManager() };
		if (!manager.HasScene(Hash(entry.key))) {
			SerializedScene factory_scene{ serialized_scene };
			if (!manager.EnterFactory(
					entry.key,
					impl::MakeSceneFactory(
						std::move(factory_scene),
						false
					)
				)) {
				current_project->scenes.erase(
					current_project->scenes.begin() +
						static_cast<std::ptrdiff_t>(index)
				);

				std::error_code error;
				fs::remove(absolute_path, error);
				return false;
			}
		}

		if (bootstrap_save) {
			pending_scene_bootstrap_saves_.emplace(entry.key);
		}

		SaveProjectManifest(app, *current_project);
		MarkProjectDirty();
		SyncProjectSceneOrder();

		const auto selected_uuid{
			selection.GetEntityUUID(entry.key, false)
		};
		ApplyEditorSelection(*context_, std::move(selection));
		scene_list_panel_.QueueSceneSelection(
			*context_,
			entry.key,
			false,
			selected_uuid
		);
		return true;
	};

	auto remove_scene = [this, entry](EditorSelection selection) mutable {
		auto* current_project{ GetProject() };
		if (!current_project) {
			return false;
		}

		const auto it{
			std::ranges::find_if(
				current_project->scenes,
				[&entry](const ProjectSceneEntry& candidate) {
					return candidate.key == entry.key;
				}
			)
		};
		if (it == current_project->scenes.end()) {
			return false;
		}

		auto& manager{ GetSceneManager() };
		if (manager.HasScene(Hash(entry.key)) &&
			!manager.Exit(entry.key)) {
			return false;
		}

		pending_scene_bootstrap_saves_.erase(entry.key);
		const path absolute_path{
			GetProjectScenePath(*current_project, *it)
		};
		const bool was_startup{
			current_project->startup_scene_key == entry.key
		};
		current_project->scenes.erase(it);

		if (was_startup && !current_project->scenes.empty()) {
			current_project->startup_scene_key =
				current_project->scenes.front().key;
		}

		std::error_code error;
		fs::remove(absolute_path, error);
		SaveProjectManifest(app, *current_project);

		MarkProjectDirty();
		SyncProjectSceneOrder();
		ApplyEditorSelection(*context_, std::move(selection));
		return true;
	};

	if (!create_scene(after_selection)) {
		return false;
	}

	undo_stack_.PushApplied(
		"Create Scene",
		[remove_scene, before_selection]() mutable {
			remove_scene(before_selection);
		},
		[create_scene, after_selection]() mutable {
			create_scene(after_selection);
		}
	);

	return true;
}

bool Editor::DuplicateProjectScene(
	std::string_view scene_key
) {
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return false;
	}

	auto* project{ GetProject() };
	if (!project) {
		return false;
	}

	const auto* source_entry{
		FindProjectScene(*project, scene_key)
	};
	if (!source_entry) {
		return false;
	}

	auto& manager{ GetSceneManager() };
	const auto source_hash{ Hash(scene_key) };
	if (!manager.HasScene(source_hash)) {
		return false;
	}

	const auto& source_scene{ manager.GetScene(source_hash) };
	if (source_scene.IsRuntime()) {
		return false;
	}

	const SerializedScene serialized_scene{
		CaptureScene(source_scene)
	};
	const std::string duplicate_display_name{
		source_entry->display_name + " Copy"
	};
	const ProjectSceneEntry entry{
		.key = MakeUniqueProjectSceneKey(
			*project,
			source_entry->key + "Copy"
		),
		.display_name = duplicate_display_name,
		.scene_path = MakeUniqueProjectScenePath(
			*project,
			duplicate_display_name
		),
	};
	const std::size_t insertion_index{ project->scenes.size() };
	const EditorSelection before_selection{ context_->local.selection };
	EditorSelection after_selection{ before_selection };
	after_selection.selected_scene_key = entry.key;
	after_selection.selected_scene_runtime = false;
	after_selection.mode = EditorSelectionMode::SceneHierarchy;

	auto create_scene = [
		this,
		entry,
		serialized_scene,
		insertion_index
	](EditorSelection selection) mutable {
		auto* current_project{ GetProject() };
		if (!current_project || FindProjectScene(*current_project, entry.key)) {
			return false;
		}

		const path absolute_path{
			GetProjectScenePath(*current_project, entry)
		};
		SaveSceneFile(absolute_path, serialized_scene);

		const std::size_t index{
			std::min(insertion_index, current_project->scenes.size())
		};
		current_project->scenes.insert(
			current_project->scenes.begin() +
				static_cast<std::ptrdiff_t>(index),
			entry
		);

		auto& current_manager{ GetSceneManager() };
		if (!current_manager.HasScene(Hash(entry.key))) {
			SerializedScene factory_scene{ serialized_scene };
			if (!current_manager.EnterFactory(
					entry.key,
					impl::MakeSceneFactory(
						std::move(factory_scene),
						false
					)
				)) {
				current_project->scenes.erase(
					current_project->scenes.begin() +
						static_cast<std::ptrdiff_t>(index)
				);
				std::error_code error;
				fs::remove(absolute_path, error);
				return false;
			}
		}

		SaveProjectManifest(app, *current_project);
		MarkProjectDirty();
		SyncProjectSceneOrder();

		const auto selected_uuid{
			selection.GetEntityUUID(entry.key, false)
		};
		ApplyEditorSelection(*context_, std::move(selection));
		scene_list_panel_.QueueSceneSelection(
			*context_,
			entry.key,
			false,
			selected_uuid
		);
		return true;
	};

	auto remove_scene = [this, entry](EditorSelection selection) mutable {
		auto* current_project{ GetProject() };
		if (!current_project) {
			return false;
		}

		const auto it{
			std::ranges::find_if(
				current_project->scenes,
				[&entry](const ProjectSceneEntry& candidate) {
					return candidate.key == entry.key;
				}
			)
		};
		if (it == current_project->scenes.end()) {
			return false;
		}

		auto& current_manager{ GetSceneManager() };
		if (current_manager.HasScene(Hash(entry.key)) &&
			!current_manager.Exit(entry.key)) {
			return false;
		}

		const path absolute_path{
			GetProjectScenePath(*current_project, *it)
		};
		current_project->scenes.erase(it);
		std::error_code error;
		fs::remove(absolute_path, error);
		SaveProjectManifest(app, *current_project);

		MarkProjectDirty();
		SyncProjectSceneOrder();
		ApplyEditorSelection(*context_, std::move(selection));
		return true;
	};

	if (!create_scene(after_selection)) {
		return false;
	}

	undo_stack_.PushApplied(
		"Duplicate Scene",
		[remove_scene, before_selection]() mutable {
			remove_scene(before_selection);
		},
		[create_scene, after_selection]() mutable {
			create_scene(after_selection);
		}
	);

	return true;
}

bool Editor::DeleteProjectScene(
	std::string_view scene_key
) {
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return false;
	}

	auto* project{ GetProject() };
	if (!project || project->scenes.size() <= 1) {
		return false;
	}

	const auto it{
		std::ranges::find_if(
			project->scenes,
			[scene_key](const ProjectSceneEntry& entry) {
				return entry.key == scene_key;
			}
		)
	};
	if (it == project->scenes.end()) {
		return false;
	}

	const std::size_t index{
		static_cast<std::size_t>(
			std::distance(project->scenes.begin(), it)
		)
	};
	const ProjectSceneEntry entry{ *it };
	const path absolute_path{
		GetProjectScenePath(*project, entry)
	};

	SerializedScene serialized_scene;
	auto& manager{ GetSceneManager() };
	if (manager.HasScene(Hash(scene_key))) {
		const auto& scene{ manager.GetScene(Hash(scene_key)) };
		if (scene.IsRuntime()) {
			return false;
		}
		serialized_scene = CaptureScene(scene);
	} else if (FileExists(absolute_path)) {
		serialized_scene = LoadSceneFile(absolute_path);
	} else {
		return false;
	}

	const bool bootstrap_save{
		pending_scene_bootstrap_saves_.contains(entry.key)
	};
	const std::string before_startup{
		project->startup_scene_key
	};
	std::string after_startup{ before_startup };
	if (before_startup == entry.key) {
		const std::size_t replacement_index{
			index + 1 < project->scenes.size()
				? index + 1
				: index - 1
		};
		after_startup = project->scenes[replacement_index].key;
	}

	const EditorSelection before_selection{
		context_->local.selection
	};
	EditorSelection after_selection{ before_selection };
	const bool deleted_scene_selected{
		after_selection.selected_scene_key == entry.key &&
		!after_selection.selected_scene_runtime
	};
	after_selection.RemoveScene(entry.key);

	if (deleted_scene_selected) {
		const std::size_t replacement_index{
			index + 1 < project->scenes.size()
				? index + 1
				: index - 1
		};
		after_selection.selected_scene_key =
			project->scenes[replacement_index].key;
		after_selection.selected_scene_runtime = false;
		after_selection.mode = EditorSelectionMode::SceneHierarchy;
	}

	auto remove_scene = [
		this,
		entry,
		after_startup
	](EditorSelection selection) mutable {
		auto* current_project{ GetProject() };
		if (!current_project || current_project->scenes.size() <= 1) {
			return false;
		}

		const auto current_it{
			std::ranges::find_if(
				current_project->scenes,
				[&entry](const ProjectSceneEntry& candidate) {
					return candidate.key == entry.key;
				}
			)
		};
		if (current_it == current_project->scenes.end()) {
			return false;
		}

		auto& current_manager{ GetSceneManager() };
		if (current_manager.HasScene(Hash(entry.key)) &&
			!current_manager.Exit(entry.key)) {
			return false;
		}

		pending_scene_bootstrap_saves_.erase(entry.key);
		const path current_absolute_path{
			GetProjectScenePath(*current_project, *current_it)
		};
		current_project->scenes.erase(current_it);
		current_project->startup_scene_key = after_startup;

		std::error_code error;
		fs::remove(current_absolute_path, error);
		SaveProjectManifest(app, *current_project);

		MarkProjectDirty();
		SyncProjectSceneOrder();
		ApplyEditorSelection(*context_, std::move(selection));
		return true;
	};

	auto restore_scene = [
		this,
		entry,
		serialized_scene,
		index,
		bootstrap_save,
		before_startup
	](EditorSelection selection) mutable {
		auto* current_project{ GetProject() };
		if (!current_project || FindProjectScene(*current_project, entry.key)) {
			return false;
		}

		const path current_absolute_path{
			GetProjectScenePath(*current_project, entry)
		};
		SaveSceneFile(current_absolute_path, serialized_scene);

		const std::size_t insertion_index{
			std::min(index, current_project->scenes.size())
		};
		current_project->scenes.insert(
			current_project->scenes.begin() +
				static_cast<std::ptrdiff_t>(insertion_index),
			entry
		);

		auto& current_manager{ GetSceneManager() };
		if (!current_manager.HasScene(Hash(entry.key))) {
			SerializedScene factory_scene{ serialized_scene };
			if (!current_manager.EnterFactory(
					entry.key,
					impl::MakeSceneFactory(
						std::move(factory_scene),
						false
					)
				)) {
				current_project->scenes.erase(
					current_project->scenes.begin() +
						static_cast<std::ptrdiff_t>(insertion_index)
				);
				return false;
			}
		}

		current_project->startup_scene_key = before_startup;
		if (bootstrap_save) {
			pending_scene_bootstrap_saves_.emplace(entry.key);
		}

		SaveProjectManifest(app, *current_project);
		MarkProjectDirty();
		SyncProjectSceneOrder();

		const bool select_restored_scene{
			selection.selected_scene_key == entry.key &&
			!selection.selected_scene_runtime
		};
		const auto selected_uuid{
			selection.GetEntityUUID(entry.key, false)
		};
		ApplyEditorSelection(*context_, selection);

		if (select_restored_scene) {
			scene_list_panel_.QueueSceneSelection(
				*context_,
				entry.key,
				false,
				selected_uuid
			);
		}
		return true;
	};

	if (!remove_scene(after_selection)) {
		return false;
	}

	undo_stack_.PushApplied(
		"Delete Scene",
		[restore_scene, before_selection]() mutable {
			restore_scene(before_selection);
		},
		[remove_scene, after_selection]() mutable {
			remove_scene(after_selection);
		}
	);

	return true;
}

bool Editor::RenameProjectSceneKey(
	std::string_view current_key,
	std::string_view new_key
) {
	PTGN_ASSERT(context_);

	if (IsPlaying() || current_key.empty() || new_key.empty()) {
		return false;
	}

	if (current_key == new_key) {
		return true;
	}

	auto apply = [this](std::string_view from, std::string_view to) {
		auto* project{ GetProject() };
		if (!project || from.empty() || to.empty() || FindProjectScene(*project, to)) {
			return false;
		}

		auto* entry{ FindProjectScene(*project, from) };
		if (!entry || !GetSceneManager().RenameScene(from, to)) {
			return false;
		}

		const std::string old_key{ entry->key };
		const bool was_startup{ project->startup_scene_key == old_key };

		entry->key = std::string{ to };
		if (was_startup) {
			project->startup_scene_key = entry->key;
		}

		if (pending_scene_bootstrap_saves_.erase(old_key) > 0) {
			pending_scene_bootstrap_saves_.emplace(entry->key);
		}

		context_->local.selection.RenameScene(old_key, entry->key);
		scene_list_panel_.RefreshSelectedSceneState();
		SaveProjectManifest(app, *project);
		MarkProjectDirty();
		SyncProjectSceneOrder();
		return true;
	};

	const std::string before{ current_key };
	const std::string after{ new_key };
	if (!apply(before, after)) {
		return false;
	}

	undo_stack_.PushApplied(
		"Rename Scene Key",
		[apply, before, after]() mutable {
			apply(after, before);
		},
		[apply, before, after]() mutable {
			apply(before, after);
		}
	);

	return true;
}

bool Editor::RenameProjectSceneDisplayName(
	std::string_view scene_key,
	std::string display_name
) {
	PTGN_ASSERT(context_);

	if (IsPlaying() || display_name.empty()) {
		return false;
	}

	auto* project{ GetProject() };
	if (!project) {
		return false;
	}

	auto* entry{ FindProjectScene(*project, scene_key) };
	if (!entry) {
		return false;
	}

	const std::string before{ entry->display_name };
	const std::string after{ std::move(display_name) };
	if (before == after) {
		return true;
	}

	const std::string key{ scene_key };
	auto apply = [this, key](const std::string& value) {
		auto* current_project{ GetProject() };
		if (!current_project) {
			return false;
		}

		auto* current_entry{ FindProjectScene(*current_project, key) };
		if (!current_entry || value.empty()) {
			return false;
		}

		current_entry->display_name = value;
		SaveProjectManifest(app, *current_project);
		MarkProjectDirty();
		return true;
	};

	if (!apply(after)) {
		return false;
	}

	undo_stack_.PushApplied(
		"Rename Scene Display Name",
		[apply, before]() mutable {
			apply(before);
		},
		[apply, after]() mutable {
			apply(after);
		}
	);

	return true;
}

bool Editor::MoveProjectScene(
	std::size_t from_index,
	std::size_t to_index
) {
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return false;
	}

	auto* project{ GetProject() };

	if (!project ||
		from_index >= project->scenes.size() ||
		to_index >= project->scenes.size()) {
		return false;
	}

	if (from_index == to_index) {
		return true;
	}

	if (from_index < to_index) {
		std::rotate(
			project->scenes.begin() +
				static_cast<std::ptrdiff_t>(from_index),
			project->scenes.begin() +
				static_cast<std::ptrdiff_t>(from_index + 1),
			project->scenes.begin() +
				static_cast<std::ptrdiff_t>(to_index + 1)
		);
	} else {
		std::rotate(
			project->scenes.begin() +
				static_cast<std::ptrdiff_t>(to_index),
			project->scenes.begin() +
				static_cast<std::ptrdiff_t>(from_index),
			project->scenes.begin() +
				static_cast<std::ptrdiff_t>(from_index + 1)
		);
	}

	MarkProjectDirty();
	SyncProjectSceneOrder();

	return true;
}

bool Editor::SetStartupProjectScene(
	std::string_view scene_key
) {
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return false;
	}

	auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	const auto* entry{
		FindProjectScene(
			*project,
			scene_key
		)
	};

	if (!entry) {
		return false;
	}

	project->startup_scene_key = entry->key;
	SaveProjectManifest(app, *project);
	MarkProjectDirty();

	return true;
}

bool Editor::IsStartupProjectScene(
	std::string_view scene_key
) const {
	const auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	const auto* entry{
		FindProjectScene(
			*project,
			scene_key
		)
	};

	return entry &&
		   entry->key == project->startup_scene_key;
}

void Editor::SavePendingBootstrapScenes() {
	if (pending_scene_bootstrap_saves_.empty()) {
		return;
	}

	auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};

	if (!app_context.project.has_value()) {
		pending_scene_bootstrap_saves_.clear();
		return;
	}

	auto& project{ app_context.project.value() };
	auto& scene_manager{ GetSceneManager() };

	std::vector<const Scene*> scenes;
	std::vector<std::string> saved_tags;

	scenes.reserve(
		pending_scene_bootstrap_saves_.size()
	);

	saved_tags.reserve(
		pending_scene_bootstrap_saves_.size()
	);

	for (const auto& scene_tag :
		 pending_scene_bootstrap_saves_) {
		auto scene_hash{ Hash(scene_tag) };

		if (!scene_manager.HasScene(scene_hash)) {
			continue;
		}

		auto& scene{
			scene_manager.GetScene(scene_hash)
		};

		if (scene.IsRuntime()) {
			continue;
		}

		PTGN_ASSERT(
			FindProjectScene(project, scene_tag),
			"Pending bootstrap scene is not in the project manifest: ",
			scene_tag
		);

		scenes.emplace_back(&scene);
		saved_tags.emplace_back(scene_tag);
	}

	if (scenes.empty()) {
		return;
	}

	if (!impl::SaveProjectScenes(
			app,
			std::span<const Scene* const>{ scenes }
		)) {
		return;
	}

	for (const auto& scene_tag : saved_tags) {
		pending_scene_bootstrap_saves_.erase(
			scene_tag
		);
	}
}

void Editor::SyncProjectSceneOrder() {
	const auto* project{ GetProject() };

	if (!project) {
		return;
	}

	std::vector<std::string> ordered_keys;
	ordered_keys.reserve(project->scenes.size());

	for (const auto& entry : project->scenes) {
		ordered_keys.emplace_back(entry.key);
	}

	GetSceneManager().ReorderScenes(
		ordered_keys,
		false
	);
}

void Editor::DrawMainMenuBar() {
	PTGN_ASSERT(context_);

	if (!ImGui::BeginMenuBar()) {
		return;
	}

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Save", nullptr, false, CanSaveProject())) {
			SaveProjectScene();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Project Settings")) {
			settings_window_.Open(SettingsPage::ProjectDisplay);
		}

		if (ImGui::MenuItem("Editor Settings")) {
			settings_window_.Open(SettingsPage::EditorGeneral);
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Edit")) {
		if (ImGui::MenuItem("Undo", "Ctrl+Z", false, undo_stack_.CanUndo())) {
			context_->local.position_picker.Cancel();
			undo_stack_.Undo();
			scene_asset_dependencies_dirty_ = true;
		}

		if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, undo_stack_.CanRedo())) {
			context_->local.position_picker.Cancel();
			undo_stack_.Redo();
			scene_asset_dependencies_dirty_ = true;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Undo History")) {
			undo_history_window_.Open();
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("View")) {
		auto render_only_selected_scene{
			GetSettings().render_only_selected_scene
		};

		if (ImGui::MenuItem(
				"Render Only Selected Scene",
				nullptr,
				render_only_selected_scene
			)) {
			SetRenderOnlySelectedScene(!render_only_selected_scene);
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Tools")) {
		auto entity_picking{ GetSettings().entity_picking };

		if (ImGui::MenuItem("Entity Picking", nullptr, entity_picking)) {
			SetEntityPickingMode(!entity_picking);
		}

		auto local_gizmo_orientation{
			GetSettings().gizmo_uses_local_orientation
		};

		if (ImGui::MenuItem(
				"Local Gizmo Orientation",
				nullptr,
				local_gizmo_orientation
			)) {
			SetGizmoUsesLocalOrientation(!local_gizmo_orientation);
		}

		auto show_read_only_data{
			context_->local.settings.show_read_only_inspector_data
		};

		if (ImGui::MenuItem(
				"Show Read-Only Data",
				nullptr,
				show_read_only_data
			)) {
			context_->local.settings.show_read_only_inspector_data =
				!show_read_only_data;
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Debug")) {
		auto& debug_settings{ GetDebugSystem().settings };

		if (ImGui::BeginMenu("Draw")) {
			if (ImGui::MenuItem(
					"Interactions",
					nullptr,
					debug_settings.interaction.draw_enabled
				)) {
				debug_settings.interaction.draw_enabled =
					!debug_settings.interaction.draw_enabled;
				MarkProjectDirty();
			}

			if (ImGui::MenuItem(
					"Collisions",
					nullptr,
					debug_settings.collision.draw_enabled
				)) {
				debug_settings.collision.draw_enabled =
					!debug_settings.collision.draw_enabled;
				MarkProjectDirty();
			}

			if (ImGui::MenuItem(
					"Text Boxes",
					nullptr,
					debug_settings.text.draw_enabled
				)) {
				debug_settings.text.draw_enabled =
					!debug_settings.text.draw_enabled;
				MarkProjectDirty();
			}

			if (ImGui::MenuItem(
					"Visibility Polygons",
					nullptr,
					debug_settings.light.draw_enabled
				)) {
				debug_settings.light.draw_enabled =
					!debug_settings.light.draw_enabled;
				MarkProjectDirty();
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Debug Settings")) {
			settings_window_.Open(SettingsPage::DebugInteraction);
		}

		if (ImGui::MenuItem(
				"ImGui Metrics",
				nullptr,
				context_->local.settings.show_imgui_metrics
			)) {
			context_->local.settings.show_imgui_metrics =
				!context_->local.settings.show_imgui_metrics;
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void Editor::DrawPanels() {
	PTGN_ASSERT(context_);

	scene_list_panel_.ClearInvalidSceneSelection(
		*context_
	);

	if (scene_list_panel_.ResolvePendingSceneSelection(*context_)) {
		auto* scene{ scene_list_panel_.GetSelectedScene() };

		viewport_panel_.SetUseEditorCamera(
			!scene || !scene->IsRuntime()
		);
	}

	// Needs to be rendered first so that the additional draw call can be displayed in the render
	// stats.
	viewport_panel_.OnRender(*context_);
	scene_hierarchy_panel_.OnRender(*context_);
	scene_list_panel_.OnRender(*context_);
	inspector_panel_.OnRender(*context_);
	if (ConsumeAcceptedAssetKeyDrop()) {
		scene_asset_dependencies_dirty_ = true;
	}
	SyncSelectedSceneAssetDependencies();
	content_browser_panel_.OnRender(*context_);
	settings_window_.OnRender(*context_);
	undo_history_window_.OnRender(*context_, undo_stack_);

	if (context_->local.settings.show_imgui_metrics) {
		ImGui::ShowMetricsWindow(&context_->local.settings.show_imgui_metrics);
	}
}

const ::ptgn::impl::SceneManager& Editor::GetSceneManager() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).scene_manager;
}

::ptgn::impl::SceneManager& Editor::GetSceneManager() {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).scene_manager;
}

SceneHierarchyPanel& Editor::GetSceneHierarchyPanel() {
	return scene_hierarchy_panel_;
}

SceneListPanel& Editor::GetSceneListPanel() {
	return scene_list_panel_;
}

bool Editor::ShouldEnableEntityPicking() const {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->local.settings.entity_picking && render_enabled_;
}

::ptgn::impl::FramebufferId Editor::GetSceneFramebuffer(Scene& scene) const {
	auto render_target{ scene.GetRenderTarget() };

	return static_cast<::ptgn::impl::FramebufferId>(render_target.Get<::ptgn::impl::FramebufferObject>());
}

void Editor::OnSelectedSceneChanged(
	Scene* previous_scene,
	Scene* selected_scene
) {
	if (previous_scene) {
		SetSceneEntityPickingEnabled(
			*previous_scene,
			false
		);
	}

	if (selected_scene) {
		SetSceneEntityPickingEnabled(
			*selected_scene,
			ShouldEnableEntityPicking()
		);
	}

	scene_asset_dependencies_dirty_ = true;
	ApplySceneRenderSettings();
}

void Editor::SyncSelectedSceneAssetDependencies() {
	if (!scene_asset_dependencies_dirty_) {
		return;
	}

	scene_asset_dependencies_dirty_ = false;

	auto* scene{ scene_list_panel_.GetSelectedScene() };
	if (!scene || scene->IsRuntime()) {
		return;
	}

	if (scene->SyncAssetDependenciesFromSerialization()) {
		context_->local.state.is_dirty = true;
	}
}

void Editor::EnableRendering(
	bool enable
) {
	render_enabled_ = enable;

	auto& window{
		GetWindow()
	};

	window.SetSetting(
		render_enabled_
			? WindowSetting::Maximized
			: WindowSetting::Restored
	);

	if (render_enabled_) {
		dock_layout_update_requested_ =
			true;

		// Maximizing may produce multiple window size updates.
		dock_resize_frames_remaining_ =
			4;
	} else {
		auto& renderer{
			GetRenderer()
		};

		renderer.SetPresentationViewport(
			std::nullopt
		);

		renderer.SetPrimaryWorldCamera(
			std::nullopt
		);
	}

	ApplyEntityPickingSettings();
	ApplySceneRenderSettings();
}

void Editor::OnUpdate() {
	undo_stack_.SetUndoRedoEnabled(!CanPause());

	scene_list_panel_.ClearInvalidSceneSelection(
		*context_
	);

	UpdateProjectLocalState();

	if (!IsPlaying()) {
		SyncProjectSceneOrder();
	}

	if (ImGui::IsKeyPressed(
			ImGuiKey_F10
		)) {
		EnableRendering(
			!render_enabled_
		);
	}

	const auto& io{
		ImGui::GetIO()
	};

	if ((io.KeyCtrl || io.KeySuper) &&
		ImGui::IsKeyPressed(
			ImGuiKey_S,
			false
		) &&
		CanSaveProject()) {
		SaveProjectScene();
	}

	if (
		undo_stack_.IsUndoRedoEnabled() &&
		(io.KeyCtrl || io.KeySuper) &&
		!io.WantTextInput
	) {
		if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			context_->local.position_picker.Cancel();
			if (io.KeyShift) {
				undo_stack_.Redo();
			} else {
				undo_stack_.Undo();
			}
			scene_asset_dependencies_dirty_ = true;
		} else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			context_->local.position_picker.Cancel();
			undo_stack_.Redo();
			scene_asset_dependencies_dirty_ = true;
		}
	}

	SavePendingBootstrapScenes();

	// Keep newly entered, removed, or replaced scenes synchronized with
	// the current selection and editor setting.
	ApplySceneRenderSettings();

	if (auto* scene{
			scene_list_panel_
				.GetSelectedScene()
		}) {
		SetSceneEntityPickingEnabled(
			*scene,
			ShouldEnableEntityPicking()
		);
	}
}

const EditorSettings& Editor::GetSettings() const {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->local.settings;
}

EditorSettings& Editor::GetSettings() {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->local.settings;
}

void Editor::SetGizmoUsesLocalOrientation(bool enabled) {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	context_->local.settings.gizmo_uses_local_orientation = enabled;
}

void Editor::SetEntityPickingMode(bool enabled) {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	if (context_->local.settings.entity_picking == enabled) {
		return;
	}

	auto previously_enabled{ ShouldEnableEntityPicking() };

	context_->local.settings.entity_picking = enabled;

	auto currently_enabled{ ShouldEnableEntityPicking() };

	if (previously_enabled == currently_enabled) {
		return;
	}

	ApplyEntityPickingSettings();
}

void Editor::ApplyEntityPickingSettings() {
	auto* scene{ scene_list_panel_.GetSelectedScene() };

	if (!scene) {
		return;
	}

	SetSceneEntityPickingEnabled(*scene, ShouldEnableEntityPicking());
}

void Editor::Play() {
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return;
	}

	auto* selected_scene{
		scene_list_panel_.GetSelectedScene()
	};

	if (!selected_scene ||
		selected_scene->IsRuntime()) {
		return;
	}

	auto* project{ GetProject() };

	if (!project) {
		return;
	}

	const auto selected_entity_uuid{
		GetSelectedEntityUUID(
			scene_hierarchy_panel_,
			selected_scene
		)
	};

	auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};
	auto& manager{ GetSceneManager() };

	app_context.runtime_project_scenes.clear();
	app_context.runtime_project_scenes.reserve(
		project->scenes.size()
	);

	for (const auto& entry : project->scenes) {
		const auto scene_hash{ Hash(entry.key) };

		PTGN_ASSERT(
			manager.HasScene(scene_hash),
			"Project scene is not loaded in the editor: ",
			entry.key
		);

		const auto& scene{
			manager.GetScene(scene_hash)
		};

		PTGN_ASSERT(
			!scene.IsRuntime(),
			"Cannot begin editor play with an existing runtime scene: ",
			entry.key
		);

		app_context.runtime_project_scenes.emplace_back(
			impl::RuntimeProjectSceneSnapshot{
				.key = entry.key,
				.scene = CaptureScene(scene),
			}
		);
	}

	const std::string selected_key{
		selected_scene->GetTag()
	};

	const auto snapshot_it{
		std::ranges::find_if(
			app_context.runtime_project_scenes,
			[&selected_key](
				const impl::RuntimeProjectSceneSnapshot& snapshot
			) {
				return snapshot.key == selected_key;
			}
		)
	};

	PTGN_ASSERT(
		snapshot_it !=
			app_context.runtime_project_scenes.end(),
		"Selected project scene snapshot is missing: ",
		selected_key
	);

	play_snapshot_ = PlaySnapshot{
		.selected_scene_key = selected_key,
		.was_dirty = context_->local.state.is_dirty,
	};

	context_->local.position_picker.Cancel();
	undo_stack_.CommitActiveEdit();
	undo_stack_.SetUndoRedoEnabled(false);

	SetApplicationState(
		ApplicationState::Running
	);

	if (!manager.ReEnterFactory(
			selected_key,
			impl::MakeSceneFactory(
				snapshot_it->scene,
				true
			)
		)) {
		undo_stack_.SetUndoRedoEnabled(true);
		app_context.runtime_project_scenes.clear();
		play_snapshot_.reset();
		return;
	}

	for (const auto& entry : project->scenes) {
		if (entry.key == selected_key) {
			continue;
		}

		PTGN_ASSERT(
			manager.Exit(entry.key),
			"Failed to remove editor scene before play: ",
			entry.key
		);
	}

	scene_list_panel_.QueueSceneSelection(
		*context_,
		selected_key,
		true,
		selected_entity_uuid
	);

	viewport_panel_.SetUseEditorCamera(false);

	context_->local.state.is_playing = true;
	context_->local.state.is_paused = false;
}

void Editor::Stop() {
	PTGN_ASSERT(context_);

	if (!IsPlaying() ||
		!play_snapshot_) {
		return;
	}

	const auto selected_entity_uuid{
		GetSelectedEntityUUID(
			scene_hierarchy_panel_,
			scene_list_panel_.GetSelectedScene()
		)
	};

	context_->local.position_picker.Cancel();

	SetApplicationState(
		ApplicationState::Running
	);

	auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};
	auto& manager{ GetSceneManager() };

	for (const auto& snapshot :
		 app_context.runtime_project_scenes) {
		const auto scene_hash{
			Hash(snapshot.key)
		};

		auto factory{
			impl::MakeSceneFactory(
				snapshot.scene,
				false
			)
		};

		const bool accepted{
			manager.HasScene(scene_hash)
				? manager.ReEnterFactory(
					snapshot.key,
					std::move(factory)
				)
				: manager.EnterFactory(
					snapshot.key,
					std::move(factory)
				)
		};

		PTGN_ASSERT(
			accepted,
			"Failed to restore project scene after editor play: ",
			snapshot.key
		);
	}

	const std::string selected_key{
		play_snapshot_->selected_scene_key
	};

	scene_list_panel_.QueueSceneSelection(
		*context_,
		selected_key,
		false,
		selected_entity_uuid
	);

	viewport_panel_.SetUseEditorCamera(true);

	context_->local.state.is_playing = false;
	context_->local.state.is_paused = false;
	undo_stack_.SetUndoRedoEnabled(true);
	context_->local.state.is_dirty =
		play_snapshot_->was_dirty;

	app_context.runtime_project_scenes.clear();
	play_snapshot_.reset();
}

void Editor::SetRenderOnlySelectedScene(
	bool enabled
) {
	PTGN_ASSERT(
		context_,
		"Editor context must be initialized"
	);

	if (context_->local.settings
			.render_only_selected_scene ==
		enabled) {
		return;
	}

	context_->local.settings
		.render_only_selected_scene =
		enabled;

	ApplySceneRenderSettings();
}

void Editor::ApplySceneRenderSettings() {
	PTGN_ASSERT(
		context_,
		"Editor context must be initialized"
	);

	auto& scenes{
		GetSceneManager().GetScenes()
	};

	Scene* selected_scene{
		scene_list_panel_.GetSelectedScene()
	};

	auto selected_it{
		std::ranges::find_if(
			scenes,
			[selected_scene](
				const auto& scene
			) {
				return
					scene &&
					scene.get() ==
						selected_scene;
			}
		)
	};

	const bool has_valid_selection{
		selected_it != scenes.end()
	};

	// Render every scene while the editor UI is hidden. This preserves
	// the normal game presentation when F10 switches out of editor view.
	//
	// Also render all scenes during a scene transition so incoming and
	// outgoing scenes can both participate in the transition.
	const bool render_only_selected{
		render_enabled_ &&
		context_->local.settings
			.render_only_selected_scene &&
		has_valid_selection &&
		!(*selected_it)->IsTransitioning()
	};

	for (auto& scene : scenes) {
		if (!scene) {
			continue;
		}

		scene->SetRenderEnabled(
			!render_only_selected ||
			scene.get() ==
				selected_scene
		);
	}
}

void Editor::TogglePause() {
	PTGN_ASSERT(context_);

	if (!CanPause()) {
		return;
	}

	context_->local.state.is_paused =
		!context_->local.state.is_paused;

	SetApplicationState(
		context_->local.state.is_paused
			? ApplicationState::Paused
			: ApplicationState::Running
	);
}

bool Editor::CanPlay() const {
	const auto* scene{
		scene_list_panel_.GetSelectedScene()
	};

	return
		!IsPlaying() &&
		scene &&
		!scene->IsRuntime();
}

bool Editor::CanStop() const {
	return
		IsPlaying() &&
		play_snapshot_.has_value();
}

bool Editor::CanPause() const {
	return std::ranges::any_of(
		GetSceneManager().GetScenes(),
		[](const auto& scene) {
			return scene &&
				   scene->IsRuntime();
		}
	);
}

bool Editor::IsDirectRuntime() const {
	return
		CanPause() &&
		!IsPlaying();
}

bool Editor::CanSaveProject() const {
	if (!context_ || IsPlaying()) {
		return false;
	}

	const auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	const auto& scene_manager{
		GetSceneManager()
	};

	return std::ranges::all_of(
		project->scenes,
		[&scene_manager](const ProjectSceneEntry& entry) {
			const auto scene_hash{ Hash(entry.key) };

			return scene_manager.HasScene(scene_hash) &&
				   !scene_manager.GetScene(scene_hash)
						.IsRuntime();
		}
	);
}

void Editor::SaveProjectScene() {
	PTGN_ASSERT(context_);

	if (!CanSaveProject()) {
		return;
	}

	const auto* project{ GetProject() };
	PTGN_ASSERT(project);

	auto& scene_manager{ GetSceneManager() };

	std::vector<const Scene*> scenes;
	scenes.reserve(project->scenes.size());

	for (const auto& entry : project->scenes) {
		const auto scene_hash{ Hash(entry.key) };

		PTGN_ASSERT(
			scene_manager.HasScene(scene_hash),
			"Project scene is not loaded in the editor: ",
			entry.key
		);

		const auto& scene{
			scene_manager.GetScene(scene_hash)
		};

		PTGN_ASSERT(
			!scene.IsRuntime(),
			"Cannot save a runtime scene: ",
			entry.key
		);

		scenes.emplace_back(&scene);
	}

	if (!impl::SaveProjectScenes(
			app,
			std::span<const Scene* const>{ scenes }
		)) {
		return;
	}

	context_->local.state.is_dirty = false;
}

bool Editor::IsPlaying() const {
	PTGN_ASSERT(context_);

	return context_->local.state.is_playing;
}

bool Editor::IsPaused() const {
	PTGN_ASSERT(context_);

	return context_->local.state.is_paused;
}

void Editor::SetTimeScale(float time_scale) {
	impl::ApplicationAccessor::ctx(app).time_scale = std::max(0.0f, time_scale);
}

float Editor::GetTimeScale() const {
	return impl::ApplicationAccessor::ctx(app).time_scale;
}

void Editor::RequestStep() {
	impl::ApplicationAccessor::ctx(app).step_requested = true;
}

Window& Editor::GetWindow() {
	return impl::ApplicationAccessor::ctx(app).window;
}

const Window& Editor::GetWindow() const {
	return impl::ApplicationAccessor::ctx(app).window;
}

const AssetManager& Editor::GetAssetManager() const {
	return impl::ApplicationAccessor::ctx(app).assets;
}

AssetManager& Editor::GetAssetManager() {
	return impl::ApplicationAccessor::ctx(app).assets;
}

const Renderer& Editor::GetRenderer() const {
	return impl::ApplicationAccessor::ctx(app).renderer;
}

Renderer& Editor::GetRenderer() {
	return impl::ApplicationAccessor::ctx(app).renderer;
}

DebugSystem& Editor::GetDebugSystem() {
	return impl::ApplicationAccessor::ctx(app).debug;
}

const DebugSystem& Editor::GetDebugSystem() const {
	return impl::ApplicationAccessor::ctx(app).debug;
}

void Editor::SetApplicationState(ApplicationState state) {
	impl::ApplicationAccessor::ctx(app).state = state;
}

ApplicationState Editor::GetApplicationState() const {
	return impl::ApplicationAccessor::ctx(app).state;
}

std::optional<path> Editor::GetProjectRoot() const {
	const auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};

	if (!app_context.project.has_value()) {
		return std::nullopt;
	}

	const auto& project_path{
		app_context.project->file_path
	};

	if (project_path.empty()) {
		return std::nullopt;
	}

	return project_path.parent_path();
}

impl::TextureId Editor::GetPresentationTexture() const {
	impl::RendererAccessor renderer{ impl::ApplicationAccessor::ctx(app).renderer };
	auto texture{ renderer.GetPresentationTexture() };
	return texture;
}

V2_int Editor::GetPresentationTextureSize() const {
	impl::RendererAccessor renderer{ impl::ApplicationAccessor::ctx(app).renderer };
	auto texture{ renderer.GetPresentationTexture() };
	return renderer.GetSize(texture).value();
}

void Editor::UpdateProjectLocalState() {
	auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};

	if (!app_context.project.has_value()) {
		local_state_project_path_.reset();
		saved_editor_local_state_json_.reset();
		return;
	}

	const auto project_path{
		app_context.project->file_path.lexically_normal()
	};

	if (local_state_project_path_.has_value() &&
		local_state_project_path_->lexically_normal() == project_path) {
		return;
	}

	local_state_project_path_ = project_path;
	OnProjectChanged();
}

void Editor::SaveEditorLocalStateIfChanged() {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};

	if (!app_context.project.has_value()) {
		return;
	}

	json value = context_->local;

	std::string serialized{ value.dump() };

	if (saved_editor_local_state_json_.has_value() &&
		saved_editor_local_state_json_.value() == serialized) {
		return;
	}

	SaveEditorLocalState(
		app_context.project.value(),
		context_->local
	);

	saved_editor_local_state_json_ =
		std::move(serialized);
}

void Editor::OnProjectChanged() {
	PTGN_ASSERT(
		context_,
		"Editor context must be initialized"
	);

	auto& app_context{
		impl::ApplicationAccessor::ctx(
			app
		)
	};

	PTGN_ASSERT(
		app_context.project.has_value(),
		"Cannot load editor local state without a project"
	);

	undo_stack_.Clear();
	play_snapshot_.reset();
	app_context.runtime_project_scenes.clear();
	pending_scene_bootstrap_saves_
		.clear();

	context_->local =
		LoadEditorLocalState(
			app_context.project.value()
		);

	context_->local.selection.selected_scene_runtime = false;
	scene_list_panel_.RefreshSelectedSceneState();

	// These values describe the current process and must never resume from disk.
	context_->local.state.is_dirty =
		false;

	context_->local.state.is_playing =
		false;

	context_->local.state.is_paused =
		false;

	json value = context_->local;

	saved_editor_local_state_json_ =
		value.dump();

	ApplyEntityPickingSettings();
	ApplySceneRenderSettings();
}

void Editor::SetSceneEntityPickingEnabled(Scene& scene, bool enabled) {
	impl::RendererAccessor renderer{ GetRenderer() };

	for (auto [entity, framebuffer] : scene.EntitiesWith<impl::FramebufferObject>()) {
		renderer.SetEntityPickingEnabled(static_cast<impl::FramebufferId>(framebuffer), enabled);
	}
}

void Editor::BuildDefaultDockLayout(std::uint32_t dockspace_id) {
	if (dock_layout_built_) {
		return;
	}

	dock_layout_built_ = true;

	auto* viewport{ ImGui::GetMainViewport() };
	auto work_size{ viewport->WorkSize };

	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, work_size);

	ImGuiID dock_main{ dockspace_id };

	dock_left_column_id_ = ImGui::DockBuilderSplitNode(
		dock_main, ImGuiDir_Left, kLeftColumnRatio, nullptr, &dock_main
	);

	// The right split ratio is relative to the space remaining after
	// removing the left column.
	float right_split_ratio{ kRightColumnRatio / (1.0f - kLeftColumnRatio) };

	dock_right_column_id_ = ImGui::DockBuilderSplitNode(
		dock_main, ImGuiDir_Right, right_split_ratio, nullptr, &dock_main
	);

	ImGuiID dock_left{};
	ImGuiID dock_left_bottom{};

	ImGui::DockBuilderSplitNode(
		dock_left_column_id_, ImGuiDir_Down, 0.35f, &dock_left_bottom, &dock_left
	);

	ImGuiID dock_right{};

	// ImGuiID dock_right_bottom{};
	ImGui::DockBuilderSplitNode(dock_right_column_id_, ImGuiDir_Down, 0.35f, nullptr, &dock_right);

	ImGuiID dock_center_bottom{};

	ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_center_bottom, &dock_main);

	ImGui::DockBuilderDockWindow("Scene Hierarchy###SceneHierarchyWindow", dock_left);
	ImGui::DockBuilderDockWindow("Prefabs###PrefabsWindow", dock_left);
	ImGui::DockBuilderDockWindow("Scenes", dock_left_bottom);

	ImGui::DockBuilderDockWindow("Inspector", dock_right);

	ImGui::DockBuilderDockWindow("Viewport", dock_main);

	ImGui::DockBuilderDockWindow("Content Browser", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Render Stats", dock_center_bottom);

	ImGui::DockBuilderFinish(dockspace_id);
}

std::size_t Editor::GetMaxTextureSlots() const {
	impl::RendererAccessor renderer{ impl::ApplicationAccessor::ctx(app).renderer };
	return renderer.GetMaxTextureSlots();
}

} // namespace ptgn::editor
