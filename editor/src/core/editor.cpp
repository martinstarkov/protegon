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
constexpr float kRightColumnRatio{ 0.30f };

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
		path{ "Scenes" } /
		(base + ".ptgnscene")
	};

	if (is_available(candidate)) {
		return candidate;
	}

	for (std::size_t index{ 2 };; ++index) {
		candidate =
			path{ "Scenes" } /
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
		.content = std::nullopt,
	};
}


} // namespace

Editor::Editor(Application& app) : app{ app } {
	// Generic application startup preference. The engine does not know why it was changed.
	impl::ApplicationAccessor::ctx(app).start_project_runtime = false;

	context_ = std::make_unique<EditorContext>(
		*this, commands_, undo_stack_
	);

	commands_ = EditorCommands{ &undo_stack_, &scene_list_panel_ };
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

	SerializedScene serialized_scene{
		MakeProjectSceneDefinition(scene_type)
	};

	ProjectSceneEntry entry{
		.key = scene_key,
		.display_name = display_name,
		.scene_path = relative_path,
	};

	const path absolute_path{
		GetProjectScenePath(*project, entry)
	};

	SaveSceneFile(
		absolute_path,
		serialized_scene
	);

	project->scenes.emplace_back(entry);

	if (!GetSceneManager().EnterFactory(
			scene_key,
			impl::MakeSceneFactory(
				std::move(serialized_scene),
				false
			)
		)) {
		project->scenes.pop_back();

		std::error_code error;
		fs::remove(
			absolute_path,
			error
		);

		return false;
	}

	SaveProjectManifest(app, *project);

	pending_scene_bootstrap_saves_.emplace(
		scene_key
	);

	scene_list_panel_.QueueSceneSelection(
		*context_,
		scene_key,
		false
	);

	MarkProjectDirty();
	SyncProjectSceneOrder();

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
		FindProjectScene(
			*project,
			scene_key
		)
	};

	if (!source_entry) {
		return false;
	}

	auto& manager{ GetSceneManager() };
	const auto source_hash{ Hash(scene_key) };

	if (!manager.HasScene(source_hash)) {
		return false;
	}

	const auto& source_scene{
		manager.GetScene(source_hash)
	};

	if (source_scene.IsRuntime()) {
		return false;
	}

	SerializedScene serialized_scene{
		CaptureScene(source_scene)
	};

	const std::string duplicate_display_name{
		source_entry->display_name + " Copy"
	};
	const std::string duplicate_key{
		MakeUniqueProjectSceneKey(
			*project,
			source_entry->key + "Copy"
		)
	};
	const path relative_path{
		MakeUniqueProjectScenePath(
			*project,
			duplicate_display_name
		)
	};

	ProjectSceneEntry entry{
		.key = duplicate_key,
		.display_name = duplicate_display_name,
		.scene_path = relative_path,
	};

	const path absolute_path{
		GetProjectScenePath(*project, entry)
	};

	SaveSceneFile(
		absolute_path,
		serialized_scene
	);

	project->scenes.emplace_back(entry);

	if (!manager.EnterFactory(
			duplicate_key,
			impl::MakeSceneFactory(
				std::move(serialized_scene),
				false
			)
		)) {
		project->scenes.pop_back();

		std::error_code error;
		fs::remove(
			absolute_path,
			error
		);

		return false;
	}

	SaveProjectManifest(app, *project);

	scene_list_panel_.QueueSceneSelection(
		*context_,
		duplicate_key,
		false
	);

	MarkProjectDirty();
	SyncProjectSceneOrder();

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

	if (!project ||
		project->scenes.size() <= 1) {
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

	const path absolute_path{
		GetProjectScenePath(*project, *it)
	};
	const bool was_startup{
		it->key == project->startup_scene_key
	};

	auto& manager{ GetSceneManager() };
	const auto scene_hash{ Hash(scene_key) };

	if (manager.HasScene(scene_hash) &&
		!manager.Exit(scene_key)) {
		return false;
	}

	auto* selected_scene{
		scene_list_panel_.GetSelectedScene()
	};

	if (selected_scene &&
		selected_scene->GetTag() == scene_key) {
		scene_list_panel_.SetSelectedScene(
			*context_,
			nullptr
		);
		scene_hierarchy_panel_.SetSelectedEntity({});
	}

	pending_scene_bootstrap_saves_.erase(
		std::string{ scene_key }
	);

	project->scenes.erase(it);

	if (was_startup) {
		project->startup_scene_key =
			project->scenes.front().key;
	}

	SaveProjectManifest(app, *project);

	std::error_code error;
	fs::remove(
		absolute_path,
		error
	);

	MarkProjectDirty();
	SyncProjectSceneOrder();

	return true;
}

bool Editor::RenameProjectSceneKey(
	std::string_view current_key,
	std::string_view new_key
) {
	PTGN_ASSERT(context_);

	if (IsPlaying() ||
		current_key.empty() ||
		new_key.empty()) {
		return false;
	}

	auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	auto* entry{
		FindProjectScene(
			*project,
			current_key
		)
	};

	if (!entry) {
		return false;
	}

	if (current_key == new_key) {
		return true;
	}

	if (FindProjectScene(*project, new_key) ||
		!GetSceneManager().RenameScene(
			current_key,
			new_key
		)) {
		return false;
	}

	const std::string old_key{ entry->key };
	const bool was_startup{
		project->startup_scene_key == old_key
	};

	entry->key = std::string{ new_key };

	if (was_startup) {
		project->startup_scene_key = entry->key;
	}

	if (pending_scene_bootstrap_saves_.erase(old_key) > 0) {
		pending_scene_bootstrap_saves_.emplace(
			entry->key
		);
	}

	MarkProjectDirty();
	return true;
}

bool Editor::RenameProjectSceneDisplayName(
	std::string_view scene_key,
	std::string display_name
) {
	PTGN_ASSERT(context_);

	if (IsPlaying() ||
		display_name.empty()) {
		return false;
	}

	auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	auto* entry{
		FindProjectScene(
			*project,
			scene_key
		)
	};

	if (!entry) {
		return false;
	}

	if (entry->display_name == display_name) {
		return true;
	}

	entry->display_name = std::move(display_name);
	MarkProjectDirty();

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

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void Editor::DrawPanels() {
	PTGN_ASSERT(context_);

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
	engine_settings_panel_.OnRender(*context_);
	debug_settings_panel_.OnRender(*context_);
	editor_settings_panel_.OnRender(*context_);
	content_browser_panel_.OnRender(*context_);
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

	ApplySceneRenderSettings();
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

	context_->local.selection.Clear();
	undo_stack_.Clear();

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

	context_->local.selection.Clear();

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

	ImGui::DockBuilderDockWindow("Engine Settings", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Debug Settings", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Editor Settings", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Content Browser", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Render Stats", dock_center_bottom);

	ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ptgn::editor