#include "editor/editor.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cfloat>
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
#include "core/build_info.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#if !defined(__EMSCRIPTEN__)
#include "editor/export_manager.h"
#endif
#include "core/assert.h"
#include "core/log.h"
#include "editor/editor_context.h"
#include "editor/editor_selection.h"
#include "editor/editor_state.h"
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

template <typename T, typename Apply>
void PushUndoableValueChange(
	UndoStack& undo_stack,
	std::string label,
	T before,
	T after,
	Apply apply,
	bool affects_project_serialization = true
) {
	apply(after);

	undo_stack.PushApplied(
		std::move(label),
		[apply, before = std::move(before)]() mutable {
			apply(before);
		},
		[apply, after = std::move(after)]() mutable {
			apply(after);
		},
		affects_project_serialization
	);
}

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
	auto& assets{ ::ptgn::impl::ApplicationAccessor::ctx(app).assets };
	assets.RefreshCatalogFromDisk();
	project.assets = assets.GetCatalog();
	project.preload_assets = assets.GetProjectAssetDependencies();
	project.settings = GetProjectSettings(app);
	SaveProject(project);
}

[[nodiscard]] std::string SceneTypeName(
	std::string_view scene_type
) {
	if (scene_type == ::ptgn::impl::kBaseSceneType) {
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
	if (scene_type == ::ptgn::impl::kBaseSceneType) {
		return SerializedScene{
			.type = std::string{ ::ptgn::impl::kBaseSceneType },
			.parameters = json::object(),
			.assets = {},
			.preload_assets = {},
			.content = std::nullopt,
		};
	}

	const auto& registration{
		::ptgn::impl::GetSceneRegistration(scene_type)
	};

	return SerializedScene{
		.type = registration.type,
		.parameters = registration.default_parameters(),
		.assets = {},
		.preload_assets = {},
		.content = std::nullopt,
	};
}



#if !defined(__EMSCRIPTEN__)

[[nodiscard]] path NormalizeExistingPath(
	const path& value
) {
	std::error_code error;
	const path canonical{
		fs::weakly_canonical(
			value,
			error
		)
	};
	return error
		? value.lexically_normal()
		: canonical;
}

[[nodiscard]] std::optional<path> ExistingDirectoryForDialog(
	const std::string& value
) {
	if (value.empty()) {
		return std::nullopt;
	}

	path current{ value };
	std::error_code error;
	if (fs::is_regular_file(current, error)) {
		current = current.parent_path();
	}
	error.clear();

	while (!current.empty() &&
		   !fs::is_directory(current, error)) {
		error.clear();
		const path parent{ current.parent_path() };
		if (parent == current) {
			break;
		}
		current = parent;
	}

	if (!current.empty() &&
		fs::is_directory(current, error)) {
		return current;
	}

	return std::nullopt;
}

[[nodiscard]] std::optional<std::string> ValidateOutputDirectoryPath(
	std::string_view value
) {
	if (value.empty()) {
		return std::string{ "Output directory is required." };
	}

	if (value.find('\0') != std::string_view::npos) {
		return std::string{ "Paths cannot contain a NUL character." };
	}

#if defined(_WIN32)
	auto is_reserved_component = [](std::string_view component) {
		if (component.empty() || component == "." || component == "..") {
			return false;
		}

		std::string base{ component.substr(0, component.find('.')) };
		std::ranges::transform(base, base.begin(), [](unsigned char c) {
			return static_cast<char>(std::toupper(c));
		});

		if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") {
			return true;
		}
		if (base.size() == 4 &&
			(base.starts_with("COM") || base.starts_with("LPT")) &&
			base[3] >= '1' && base[3] <= '9') {
			return true;
		}
		return false;
	};

	for (std::size_t i{ 0 }; i < value.size(); ++i) {
		const unsigned char c{ static_cast<unsigned char>(value[i]) };
		if (c < 32) {
			return std::string{ "Windows paths cannot contain control characters." };
		}
		if (value[i] == '<' || value[i] == '>' || value[i] == '"' ||
			value[i] == '|' || value[i] == '?' || value[i] == '*') {
			return std::string{ "Windows paths cannot contain < > \" | ? or *." };
		}
		if (value[i] == ':' &&
			!(i == 1 && std::isalpha(static_cast<unsigned char>(value[0])) != 0)) {
			return std::string{ "A colon is only valid after a Windows drive letter." };
		}
	}

	std::size_t component_start{};
	for (std::size_t i{}; i <= value.size(); ++i) {
		const bool separator{
			i == value.size() || value[i] == '/' || value[i] == '\\'
		};
		if (!separator) {
			continue;
		}

		std::string_view component{
			value.substr(component_start, i - component_start)
		};
		component_start = i + 1;
		if (component.empty() || component == "." || component == ".." ||
			(component.size() == 2 && component[1] == ':')) {
			continue;
		}
		if (component.back() == ' ' || component.back() == '.') {
			return std::string{
				"Windows path components cannot end with a space or period."
			};
		}
		if (is_reserved_component(component)) {
			return std::string{
				"The path contains a reserved Windows device name."
			};
		}
	}
#endif

	return std::nullopt;
}

bool DrawDirectoryField(
	Editor& editor,
	const char* label,
	const char* id,
	std::string& value,
	const std::optional<std::string>& validation_error = std::nullopt
) {
	bool changed{ false };

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);

	ImGui::TableSetColumnIndex(1);
	std::array<char, 4096> buffer{};
	const std::size_t count{
		std::min(
			value.size(),
			buffer.size() - 1
		)
	};
	std::copy_n(
		value.data(),
		count,
		buffer.data()
	);

	if (validation_error.has_value()) {
		ImGui::PushStyleColor(
			ImGuiCol_Border,
			ImVec4{ 0.90f, 0.20f, 0.20f, 1.0f }
		);
		ImGui::PushStyleVar(
			ImGuiStyleVar_FrameBorderSize,
			1.0f
		);
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputText(
			id,
			buffer.data(),
			buffer.size()
		)) {
		value = buffer.data();
		changed = true;
	}
	const bool path_hovered{
		ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary)
	};

	if (validation_error.has_value()) {
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	if (path_hovered) {
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(
			value.empty() ? "<empty>" : value.c_str()
		);
		if (validation_error.has_value()) {
			ImGui::Separator();
			ImGui::TextColored(
				ImVec4{ 1.0f, 0.35f, 0.35f, 1.0f },
				"Invalid path: %s",
				validation_error->c_str()
			);
		}
		ImGui::EndTooltip();
	}

	ImGui::TableSetColumnIndex(2);
	std::string button_id{
		"Select##"
	};
	button_id += id;

	if (ImGui::Button(
			button_id.c_str(),
			ImVec2{ -FLT_MIN, 0.0f }
		)) {
		FileDialog::Options options;
		options.default_path =
			ExistingDirectoryForDialog(value);

		auto result{
			editor.GetWindow()
				.file
				.OpenFolder(options)
		};
		if (!result.has_value()) {
			PTGN_ERROR(
				"Failed to open folder dialog: ",
				result.error()
			);
		} else if (result.value().has_value()) {
			value = result.value()
				.value()
				.lexically_normal()
				.string();
			changed = true;
		}
	}

	return changed;
}

[[nodiscard]] bool DirectoryHasContent(
	const path& directory
) {
	std::error_code error;
	if (!fs::exists(directory, error) ||
		!fs::is_directory(directory, error)) {
		return false;
	}

	return fs::directory_iterator{
		directory,
		error
	} != fs::directory_iterator{};
}

[[nodiscard]] path ResolveProjectFilePath(
	const Project& project
) {
	if (project.file_path.empty()) {
		return {};
	}

	if (project.file_path.is_absolute()) {
		return NormalizeExistingPath(
			project.file_path
		);
	}

	std::error_code error;
	const path working_candidate{
		fs::absolute(
			project.file_path,
			error
		)
	};
	if (!error && fs::exists(
			working_candidate,
			error
		)) {
		return NormalizeExistingPath(
			working_candidate
		);
	}

	const auto& build_info{
		::ptgn::impl::GetBuildInfo()
	};
	std::vector<path> candidates;
	if (build_info.IsExample()) {
		candidates.emplace_back(
			build_info.binary_directory /
			"examples" /
			project.file_path
		);
	}
	candidates.emplace_back(
		build_info.binary_directory /
		project.file_path
	);
	candidates.emplace_back(
		build_info.runtime_root /
		project.file_path
	);
	candidates.emplace_back(
		build_info.source_directory /
		project.file_path
	);

	for (const auto& candidate : candidates) {
		error.clear();
		if (fs::exists(candidate, error)) {
			return NormalizeExistingPath(
				candidate
			);
		}
	}

	if (!working_candidate.empty()) {
		return working_candidate.lexically_normal();
	}

	return (
		build_info.runtime_root /
		project.file_path
	).lexically_normal();
}

[[nodiscard]] path ResolveProjectRuntimeRoot(
	const Project& project,
	const path& resolved_project_file
) {
	if (project.file_path.empty() ||
		project.file_path.is_absolute()) {
		return ::ptgn::impl::GetBuildInfo()
			.runtime_root;
	}

	path root{ resolved_project_file };
	for (const auto& component :
		 project.file_path.lexically_normal()) {
		if (component.empty() ||
			component == ".") {
			continue;
		}
		if (component == "..") {
			return ::ptgn::impl::GetBuildInfo()
				.runtime_root;
		}
		root = root.parent_path();
	}

	return root.lexically_normal();
}

[[nodiscard]] path ProjectRuntimeMount(
	const Project& project,
	const path& resolved_project_file,
	const path& resolved_runtime_root
) {
	if (project.file_path.is_relative()) {
		return project.file_path
			.parent_path()
			.lexically_normal();
	}

	const path relative{
		resolved_project_file
			.parent_path()
			.lexically_relative(
				resolved_runtime_root
			)
	};
	if (relative.empty() || relative == ".") {
		return {};
	}

	for (const auto& component : relative) {
		if (component == "..") {
			return resolved_project_file
				.parent_path()
				.filename();
		}
	}

	return relative;
}

#endif

} // namespace

Editor::Editor(Application& app) : app{ app } {
	app.SetCloseGuard([this]() {
#if !defined(__EMSCRIPTEN__)
		if (!allow_application_close_ && export_manager_.IsBusy()) {
			if (!render_enabled_) {
				EnableRendering(true);
			}
			export_window_open_ = true;
			pending_task_confirmation_ =
				PendingTaskConfirmation::CloseApplicationWhileRunning;
			task_confirmation_popup_requested_ = true;
			return false;
		}
#endif
		return content_browser_panel_.CanApplicationClose();
	});

	// Generic application startup preference. The engine does not know why it was changed.
	::ptgn::impl::ApplicationAccessor::ctx(app).start_project_runtime = false;

	context_ = std::make_unique<EditorContext>(
		*this, commands_, undo_stack_
	);

	commands_.Bind(*context_);
	scene_hierarchy_panel_.Bind(*context_);
	scene_list_panel_.Bind(*context_);

#if !defined(__EMSCRIPTEN__)
	const auto& build_info{
		::ptgn::impl::GetBuildInfo()
	};

	desktop_export_directory_ = (
		build_info.source_directory /
		"release" /
		build_info.target
	).string();

	web_export_directory_ = (
		build_info.source_directory /
		"release-web" /
		build_info.target
	).string();
#endif
}

void Editor::RequestQuit() {
	app.RequestQuit();
}

void Editor::RefreshProjectDirtyState() {
	PTGN_ASSERT(context_);

	const bool dirty{
		untracked_project_dirty_ ||
		undo_stack_.IsProjectDirty()
	};

	if (context_->local.state.is_dirty != dirty) {
		context_->local.state.is_dirty = dirty;
	}

	UpdateWindowTitle();
}

void Editor::UpdateWindowTitle() {
	auto& window{ GetWindow() };

	std::string title;

	if (!render_enabled_) {
		title = window.GetSettings().title;
	} else if (const auto* project{ GetProject() }) {
		title = "Editor: " + project->name;

		if (context_ &&
			context_->local.state.is_dirty) {
			title += " *";
		}
	} else {
		title = "Editor";
	}

	if (window.GetTitle() != title) {
		window.SetTitle(title);
	}
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

	RefreshProjectDirtyState();
	SaveEditorLocalStateIfChanged();
}

Project* Editor::GetProject() {
	return ::ptgn::impl::ApplicationAccessor::ctx(app)
		.project
		? std::addressof(
			::ptgn::impl::ApplicationAccessor::ctx(app)
				.project.value()
		)
		: nullptr;
}

const Project* Editor::GetProject() const {
	const auto& project{
		::ptgn::impl::ApplicationAccessor::ctx(app)
			.project
	};

	return project
		? std::addressof(project.value())
		: nullptr;
}

void Editor::MarkProjectDirty() {
	PTGN_ASSERT(context_);
	untracked_project_dirty_ = true;
	scene_asset_dependencies_dirty_ = true;
	RefreshProjectDirtyState();
}

bool Editor::CreateProjectScene(
	std::string_view scene_type
) {
#if defined(__EMSCRIPTEN__)
	return false;
#endif
	PTGN_ASSERT(context_);

	if (IsPlaying()) {
		return false;
	}

	auto* project{ GetProject() };

	if (!project) {
		return false;
	}

	if (scene_type != ::ptgn::impl::kBaseSceneType &&
		!::ptgn::impl::GetSceneRegistry().contains(scene_type)) {
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
					::ptgn::impl::MakeSceneFactory(
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
#if defined(__EMSCRIPTEN__)
	return false;
#endif
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
					::ptgn::impl::MakeSceneFactory(
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
#if defined(__EMSCRIPTEN__)
	return false;
#endif
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
					::ptgn::impl::MakeSceneFactory(
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
#if defined(__EMSCRIPTEN__)
	return false;
#endif
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
#if defined(__EMSCRIPTEN__)
	return false;
#endif
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
#if defined(__EMSCRIPTEN__)
	return false;
#endif
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
#if defined(__EMSCRIPTEN__)
	return false;
#endif
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
#if defined(__EMSCRIPTEN__)
	return;
#else
	if (export_manager_.IsExportingProjectFiles()) {
		return;
	}
#endif
	if (pending_scene_bootstrap_saves_.empty()) {
		return;
	}

	auto& app_context{
		::ptgn::impl::ApplicationAccessor::ctx(app)
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

	if (!::ptgn::impl::SaveProjectScenes(
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
#if !defined(__EMSCRIPTEN__)
		if (ImGui::MenuItem(
				"Save",
				nullptr,
				false,
				CanSaveProject()
			)) {
			SaveProjectScene();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Export...")) {
			OpenExportWindow();
		}

		ImGui::Separator();
#endif

#if !defined(__EMSCRIPTEN__)
		if (ImGui::MenuItem("Settings...")) {
			settings_window_.Open(
				SettingsPage::ProjectDisplay
			);
		}
#endif

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Edit")) {
		if (ImGui::MenuItem(
				"Undo",
				"Ctrl+Z",
				false,
				undo_stack_.CanUndo()
			)) {
			context_->local.position_picker.Cancel();
			undo_stack_.Undo();
			scene_asset_dependencies_dirty_ = true;
		}

		if (ImGui::MenuItem(
				"Redo",
				"Ctrl+Shift+Z",
				false,
				undo_stack_.CanRedo()
			)) {
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
		auto toggle_editor_setting =
			[this](
				std::string label,
				auto member
			) {
				EditorSettings before{ GetSettings() };
				EditorSettings after{ before };
				after.*member = !(before.*member);

				PushUndoableValueChange(
					undo_stack_,
					std::move(label),
					std::move(before),
					std::move(after),
					[this](const EditorSettings& settings) {
						SetEditorSettings(settings);
					},
					false
				);
			};

		if (ImGui::MenuItem(
				"Render Only Selected Scene",
				nullptr,
				GetSettings().render_only_selected_scene
			)) {
			toggle_editor_setting(
				"Toggle Render Only Selected Scene",
				&EditorSettings::render_only_selected_scene
			);
		}

		ImGui::Separator();

		if (ImGui::MenuItem(
				"Entity Picking",
				nullptr,
				GetSettings().entity_picking
			)) {
			toggle_editor_setting(
				"Toggle Entity Picking",
				&EditorSettings::entity_picking
			);
		}

		if (ImGui::MenuItem(
				"Local Gizmo Orientation",
				nullptr,
				GetSettings().gizmo_uses_local_orientation
			)) {
			toggle_editor_setting(
				"Toggle Local Gizmo Orientation",
				&EditorSettings::gizmo_uses_local_orientation
			);
		}

		if (ImGui::MenuItem(
				"Show Read-Only Data",
				nullptr,
				GetSettings().show_read_only_inspector_data
			)) {
			toggle_editor_setting(
				"Toggle Read-Only Inspector Data",
				&EditorSettings::show_read_only_inspector_data
			);
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Debug")) {
		auto& debug_settings{
			GetDebugSystem().settings
		};

		if (ImGui::BeginMenu("Draw")) {
			auto toggle_debug_setting =
				[this, &debug_settings](
					std::string label,
					auto toggle
				) {
					auto before{ debug_settings };
					auto after{ before };
					toggle(after);

					PushUndoableValueChange(
						undo_stack_,
						std::move(label),
						std::move(before),
						std::move(after),
						[this](const auto& settings) {
							GetDebugSystem().settings = settings;
											}
					);
				};

			if (ImGui::MenuItem(
					"Interactions",
					nullptr,
					debug_settings.interaction.draw_enabled
				)) {
				toggle_debug_setting(
					"Toggle Debug Interactions",
					[](auto& settings) {
						settings.interaction.draw_enabled =
							!settings.interaction.draw_enabled;
					}
				);
			}

			if (ImGui::MenuItem(
					"Collisions",
					nullptr,
					debug_settings.collision.draw_enabled
				)) {
				toggle_debug_setting(
					"Toggle Debug Collisions",
					[](auto& settings) {
						settings.collision.draw_enabled =
							!settings.collision.draw_enabled;
					}
				);
			}

			if (ImGui::MenuItem(
					"Text Boxes",
					nullptr,
					debug_settings.text.draw_enabled
				)) {
				toggle_debug_setting(
					"Toggle Debug Text Boxes",
					[](auto& settings) {
						settings.text.draw_enabled =
							!settings.text.draw_enabled;
					}
				);
			}

			if (ImGui::MenuItem(
					"Visibility Polygons",
					nullptr,
					debug_settings.light.draw_enabled
				)) {
				toggle_debug_setting(
					"Toggle Debug Visibility Polygons",
					[](auto& settings) {
						settings.light.draw_enabled =
							!settings.light.draw_enabled;
					}
				);
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Settings...")) {
			settings_window_.Open(
				SettingsPage::DebugInteraction
			);
		}

		if (ImGui::MenuItem(
				"ImGui Metrics",
				nullptr,
				context_->local.settings.show_imgui_metrics
			)) {
			EditorSettings before{ GetSettings() };
			EditorSettings after{ before };
			after.show_imgui_metrics =
				!before.show_imgui_metrics;

			PushUndoableValueChange(
				undo_stack_,
				"Toggle ImGui Metrics",
				std::move(before),
				std::move(after),
				[this](const EditorSettings& settings) {
					SetEditorSettings(settings);
				},
				false
			);
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

#if !defined(__EMSCRIPTEN__)

void Editor::OpenExportWindow() {
	export_window_open_ = true;
	export_window_recenter_requested_ = true;
}

ExportRequest Editor::MakeExportRequest() const {
	const auto& build_info{
		::ptgn::impl::GetBuildInfo()
	};

	ExportRequest request{
		.target = export_target_,
		.configuration = export_configuration_,
		.include_editor = export_include_editor_,
		.output_directory =
			export_target_ == ExportTarget::Desktop
				? path{ desktop_export_directory_ }
				: path{ web_export_directory_ },
		.asset_source_directory =
			build_info.asset_source_directory,
	};

	if (const auto* project{ GetProject() }) {
		const path project_file{
			ResolveProjectFilePath(*project)
		};
		const path runtime_root{
			ResolveProjectRuntimeRoot(
				*project,
				project_file
			)
		};

		request.project_directory =
			project_file.parent_path();
		request.project_mount =
			ProjectRuntimeMount(
				*project,
				project_file,
				runtime_root
			);
	}

	return request;
}

bool Editor::ExportRequestNeedsConfirmation(
	const ExportRequest& request
) const {
	return DirectoryHasContent(
		request.output_directory
	);
}

void Editor::StartExport(
	ExportRequest request
) {
	export_manager_.Export(
		std::move(request)
	);
}

void Editor::DrawExportWindow() {
	if (!export_window_open_) {
		return;
	}

	if (export_window_recenter_requested_) {
		auto* viewport{ ImGui::GetMainViewport() };
		const ImVec2 size{
			viewport->WorkSize.x * 0.60f,
			viewport->WorkSize.y * 0.70f
		};
		const ImVec2 center{
			viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
			viewport->WorkPos.y + viewport->WorkSize.y * 0.5f
		};

		ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
		ImGui::SetNextWindowPos(
			center,
			ImGuiCond_Always,
			ImVec2{ 0.5f, 0.5f }
		);
		ImGui::SetNextWindowSize(
			size,
			ImGuiCond_Always
		);
		ImGui::SetNextWindowFocus();
		export_window_recenter_requested_ = false;
	}

	bool window_open{ true };
	const bool visible{
		ImGui::Begin(
			"Export###ExportWindow",
			&window_open
		)
	};

	const bool busy{ export_manager_.IsBusy() };

	if (visible) {
		ImGui::BeginDisabled(busy);

		if (ImGui::BeginTable(
				"ExportSettingsTable",
				3,
				ImGuiTableFlags_SizingStretchProp |
					ImGuiTableFlags_NoSavedSettings
			)) {
			ImGui::TableSetupColumn(
				"Label",
				ImGuiTableColumnFlags_WidthFixed,
				175.0f
			);
			ImGui::TableSetupColumn(
				"Value",
				ImGuiTableColumnFlags_WidthStretch
			);
			ImGui::TableSetupColumn(
				"Select",
				ImGuiTableColumnFlags_WidthFixed,
				80.0f
			);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Platform");
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			const char* platform_preview{
				export_target_ == ExportTarget::Desktop
					? "Desktop"
					: "Web"
			};
			if (ImGui::BeginCombo(
					"##ExportPlatform",
					platform_preview
				)) {
				if (ImGui::Selectable(
						"Desktop",
						export_target_ == ExportTarget::Desktop
					)) {
					export_target_ = ExportTarget::Desktop;
				}
				if (ImGui::Selectable(
						"Web",
						export_target_ == ExportTarget::Web
					)) {
					export_target_ = ExportTarget::Web;
				}
				ImGui::EndCombo();
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Configuration");
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			const char* configuration_preview{
				export_configuration_ == ExportConfiguration::Debug
					? "Debug"
					: "Release"
			};
			if (ImGui::BeginCombo(
					"##ExportConfiguration",
					configuration_preview
				)) {
				if (ImGui::Selectable(
						"Debug",
						export_configuration_ == ExportConfiguration::Debug
					)) {
					export_configuration_ = ExportConfiguration::Debug;
				}
				if (ImGui::Selectable(
						"Release",
						export_configuration_ == ExportConfiguration::Release
					)) {
					export_configuration_ = ExportConfiguration::Release;
				}
				ImGui::EndCombo();
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Include Editor");
			ImGui::TableSetColumnIndex(1);
			ImGui::Checkbox(
				"##ExportIncludeEditor",
				&export_include_editor_
			);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary)) {
				ImGui::SetTooltip(
					"Build the exported executable with PTGN_EDITOR enabled."
				);
			}

			std::string& current_output_directory{
				export_target_ == ExportTarget::Desktop
					? desktop_export_directory_
					: web_export_directory_
			};
			const auto current_output_path_error{
				ValidateOutputDirectoryPath(current_output_directory)
			};
			DrawDirectoryField(
				*this,
				"Output Directory",
				"##ExportOutputDirectory",
				current_output_directory,
				current_output_path_error
			);

			ImGui::EndTable();
		}

		ImGui::EndDisabled();

		const std::string& current_output_directory{
			export_target_ == ExportTarget::Desktop
				? desktop_export_directory_
				: web_export_directory_
		};
		const auto current_output_path_error{
			ValidateOutputDirectoryPath(current_output_directory)
		};

		const path build_directory{
			export_manager_.GetBuildDirectory(
				export_target_,
				export_configuration_
			)
		};
		const bool can_clean{
			!busy && DirectoryHasContent(build_directory)
		};

		ImGui::Separator();

		ImGui::BeginDisabled(!can_clean);
		if (ImGui::Button("Clean Build Cache")) {
			pending_clean_target_ = export_target_;
			pending_clean_configuration_ = export_configuration_;
			pending_task_confirmation_ =
				PendingTaskConfirmation::Clean;
			task_confirmation_popup_requested_ = true;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		if (busy) {
			ImGui::BeginDisabled(!export_manager_.CanCancel());
			if (ImGui::Button("Cancel Export")) {
				export_manager_.Cancel();
			}
			ImGui::EndDisabled();
		} else {
			ImGui::BeginDisabled(current_output_path_error.has_value());
			if (ImGui::Button("Start Export")) {
				if (CanSaveProject()) {
					SaveProjectScene();
				}

				ExportRequest request{ MakeExportRequest() };
				if (ExportRequestNeedsConfirmation(request)) {
					pending_export_request_ = std::move(request);
					pending_task_confirmation_ =
						PendingTaskConfirmation::Export;
					task_confirmation_popup_requested_ = true;
				} else {
					StartExport(std::move(request));
				}
			}
			ImGui::EndDisabled();
		}

		ImGui::SeparatorText("Output");
		export_manager_.DrawOutputPanel();
	}

	ImGui::End();

	if (!window_open) {
		if (busy) {
			export_window_open_ = true;
			pending_task_confirmation_ =
				PendingTaskConfirmation::CloseExportWindowWhileRunning;
			task_confirmation_popup_requested_ = true;
		} else {
			export_window_open_ = false;
		}
	}
}

void Editor::DrawTaskConfirmationPopup() {
	if (task_confirmation_popup_requested_) {
		ImGui::OpenPopup(
			"Confirm Export Action###ExportTaskConfirmation"
		);
		task_confirmation_popup_requested_ = false;
	}

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 560.0f, 0.0f },
		ImVec2{ 760.0f, FLT_MAX }
	);
	if (!ImGui::BeginPopupModal(
			"Confirm Export Action###ExportTaskConfirmation",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize
		)) {
		return;
	}

	switch (pending_task_confirmation_) {
		case PendingTaskConfirmation::Export:
			ImGui::TextWrapped(
				"The output directory already contains files. Replace files owned by this export?"
			);
			break;

		case PendingTaskConfirmation::Clean:
			ImGui::TextWrapped(
				"Delete the cached export build files for the selected platform and configuration? This cannot be undone."
			);
			break;

		case PendingTaskConfirmation::CloseExportWindowWhileRunning:
			ImGui::TextWrapped(
				"An export is still running. Cancel it and close this window?"
			);
			break;

		case PendingTaskConfirmation::CloseApplicationWhileRunning:
			ImGui::TextWrapped(
				"An export is still running. Cancel it and exit the application?"
			);
			break;

		case PendingTaskConfirmation::None:
			break;
	}

	ImGui::Separator();

	if (ImGui::Button("Continue")) {
		switch (pending_task_confirmation_) {
			case PendingTaskConfirmation::Export:
				if (pending_export_request_) {
					pending_export_request_->replace_existing = true;
					StartExport(
						std::move(pending_export_request_.value())
					);
				}
				break;

			case PendingTaskConfirmation::Clean:
				if (pending_clean_target_ && pending_clean_configuration_) {
					export_manager_.Clean(
						pending_clean_target_.value(),
						pending_clean_configuration_.value()
					);
				}
				break;

			case PendingTaskConfirmation::CloseExportWindowWhileRunning:
				export_manager_.Cancel();
				export_window_open_ = false;
				break;

			case PendingTaskConfirmation::CloseApplicationWhileRunning:
				export_manager_.Cancel();
				allow_application_close_ = true;
				app.RequestQuit();
				break;

			case PendingTaskConfirmation::None:
				break;
		}

		pending_export_request_.reset();
		pending_clean_target_.reset();
		pending_clean_configuration_.reset();
		pending_task_confirmation_ = PendingTaskConfirmation::None;
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel")) {
		pending_export_request_.reset();
		pending_clean_target_.reset();
		pending_clean_configuration_.reset();
		pending_task_confirmation_ = PendingTaskConfirmation::None;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}
#endif

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

	const EditorSettings before_content_browser_settings{ GetSettings() };
	content_browser_panel_.OnRender(*context_);
	const EditorSettings after_content_browser_settings{ GetSettings() };

	if (before_content_browser_settings != after_content_browser_settings) {
		PushUndoableValueChange(
			undo_stack_,
			"Change Content Browser Settings",
			before_content_browser_settings,
			after_content_browser_settings,
			[this](const EditorSettings& settings) {
				SetEditorSettings(settings);
			},
			false
		);
	}

	settings_window_.OnRender(*context_);
	undo_history_window_.OnRender(*context_, undo_stack_);

#if !defined(__EMSCRIPTEN__)
	DrawExportWindow();
	DrawTaskConfirmationPopup();
#endif

	if (context_->local.settings.show_imgui_metrics) {
		const EditorSettings before_metrics_settings{ GetSettings() };
		ImGui::ShowMetricsWindow(&context_->local.settings.show_imgui_metrics);
		const EditorSettings after_metrics_settings{ GetSettings() };

		if (before_metrics_settings != after_metrics_settings) {
			PushUndoableValueChange(
				undo_stack_,
				"Toggle ImGui Metrics",
				before_metrics_settings,
				after_metrics_settings,
				[this](const EditorSettings& settings) {
					SetEditorSettings(settings);
				},
				false
			);
		}
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

	(void)scene->SyncAssetDependenciesFromSerialization();
}

void Editor::EnableRendering(
	bool enable
) {
	render_enabled_ = enable;

	auto& app_context{
		::ptgn::impl::ApplicationAccessor::ctx(app)
	};

	// Before a project has started, editor visibility determines how the
	// project should launch. A hidden editor starts directly in runtime.
	if (!app_context.project.has_value()) {
		app_context.start_project_runtime =
			!render_enabled_;
	}

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
	UpdateWindowTitle();
}

void Editor::OnUpdate() {
#if !defined(__EMSCRIPTEN__)
	export_manager_.OnUpdate();
#endif

	undo_stack_.SetUndoRedoEnabled(!CanPause());

	scene_list_panel_.ClearInvalidSceneSelection(
		*context_
	);

	UpdateProjectLocalState();
	UpdateRuntimeViewportState();

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
		CanSaveProject()
	) {
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

	RefreshProjectDirtyState();
}

const EditorSettings& Editor::GetSettings() const {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->local.settings;
}

EditorSettings& Editor::GetSettings() {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->local.settings;
}

void Editor::SetEditorSettings(EditorSettings settings) {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	SetEntityPickingMode(settings.entity_picking);
	SetRenderOnlySelectedScene(settings.render_only_selected_scene);
	SetGizmoUsesLocalOrientation(settings.gizmo_uses_local_orientation);

	auto& current{ context_->local.settings };
	current.show_read_only_inspector_data =
		settings.show_read_only_inspector_data;
	current.show_imgui_metrics =
		settings.show_imgui_metrics;
	current.content_browser_items_per_row =
		settings.content_browser_items_per_row;
	current.content_browser_search_entire_tree =
		settings.content_browser_search_entire_tree;
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
		::ptgn::impl::ApplicationAccessor::ctx(app)
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
			::ptgn::impl::RuntimeProjectSceneSnapshot{
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
				const ::ptgn::impl::RuntimeProjectSceneSnapshot& snapshot
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
			::ptgn::impl::MakeSceneFactory(
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
		::ptgn::impl::ApplicationAccessor::ctx(app)
	};
	auto& manager{ GetSceneManager() };

	for (const auto& snapshot :
		 app_context.runtime_project_scenes) {
		const auto scene_hash{
			Hash(snapshot.key)
		};

		auto factory{
			::ptgn::impl::MakeSceneFactory(
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
	RefreshProjectDirtyState();
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
#if defined(__EMSCRIPTEN__)
	return false;
#else
	if (export_manager_.IsExportingProjectFiles()) {
		return false;
	}
#endif

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

	undo_stack_.CommitActiveEdit();

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

	if (!::ptgn::impl::SaveProjectScenes(
			app,
			std::span<const Scene* const>{ scenes }
		)) {
		return;
	}

	undo_stack_.MarkProjectSaved();
	untracked_project_dirty_ = false;
	RefreshProjectDirtyState();
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
	::ptgn::impl::ApplicationAccessor::ctx(app).time_scale = std::max(0.0f, time_scale);
}

float Editor::GetTimeScale() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).time_scale;
}

void Editor::RequestStep() {
	::ptgn::impl::ApplicationAccessor::ctx(app).step_requested = true;
}

Window& Editor::GetWindow() {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).window;
}

const Window& Editor::GetWindow() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).window;
}

const AssetManager& Editor::GetAssetManager() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).assets;
}

AssetManager& Editor::GetAssetManager() {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).assets;
}

const Renderer& Editor::GetRenderer() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).renderer;
}

Renderer& Editor::GetRenderer() {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).renderer;
}

DebugSystem& Editor::GetDebugSystem() {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).debug;
}

const DebugSystem& Editor::GetDebugSystem() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).debug;
}

void Editor::SetApplicationState(ApplicationState state) {
	::ptgn::impl::ApplicationAccessor::ctx(app).state = state;
}

ApplicationState Editor::GetApplicationState() const {
	return ::ptgn::impl::ApplicationAccessor::ctx(app).state;
}

std::optional<path> Editor::GetProjectRoot() const {
	const auto& app_context{
		::ptgn::impl::ApplicationAccessor::ctx(app)
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

::ptgn::impl::TextureId Editor::GetPresentationTexture() const {
	::ptgn::impl::RendererAccessor renderer{ ::ptgn::impl::ApplicationAccessor::ctx(app).renderer };
	auto texture{ renderer.GetPresentationTexture() };
	return texture;
}

V2_int Editor::GetPresentationTextureSize() const {
	::ptgn::impl::RendererAccessor renderer{ ::ptgn::impl::ApplicationAccessor::ctx(app).renderer };
	auto texture{ renderer.GetPresentationTexture() };
	return renderer.GetSize(texture).value();
}

void Editor::UpdateRuntimeViewportState() {
	bool runtime_active{
		IsPlaying() || CanPause()
	};

	if (runtime_active && !runtime_was_active_) {
		viewport_panel_.SetUseEditorCamera(false);
	}

	runtime_was_active_ = runtime_active;
}

void Editor::UpdateProjectLocalState() {
	auto& app_context{
		::ptgn::impl::ApplicationAccessor::ctx(app)
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
#if defined(__EMSCRIPTEN__)
	return;
#endif

	PTGN_ASSERT(context_, "Editor context must be initialized");

	auto& app_context{
		::ptgn::impl::ApplicationAccessor::ctx(app)
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
		::ptgn::impl::ApplicationAccessor::ctx(
			app
		)
	};

	PTGN_ASSERT(
		app_context.project.has_value(),
		"Cannot load editor local state without a project"
	);

	undo_stack_.Clear();
	undo_stack_.MarkProjectSaved();
	untracked_project_dirty_ = false;
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
	RefreshProjectDirtyState();
}

void Editor::SetSceneEntityPickingEnabled(Scene& scene, bool enabled) {
	::ptgn::impl::RendererAccessor renderer{ GetRenderer() };

	for (auto [entity, framebuffer] : scene.EntitiesWith<::ptgn::impl::FramebufferObject>()) {
		renderer.SetEntityPickingEnabled(static_cast<::ptgn::impl::FramebufferId>(framebuffer), enabled);
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
	::ptgn::impl::RendererAccessor renderer{ ::ptgn::impl::ApplicationAccessor::ctx(app).renderer };
	return renderer.GetMaxTextureSlots();
}

} // namespace ptgn::editor
