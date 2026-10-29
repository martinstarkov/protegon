#include "app/project.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/application.h"
#include "app/application_context.h"
#include "core/assert.h"
#include "core/util/file.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

bool ScenePathsEqual(const path& a, const path& b) {
	return a.lexically_normal().generic_string() == b.lexically_normal().generic_string();
}

std::string TypeNameWithoutNamespaces(std::string_view type) {
	if (type == impl::kBaseSceneType) {
		return "Scene";
	}

	auto separator{ type.rfind("::") };
	return separator == std::string_view::npos ? std::string{ type }
											   : std::string{ type.substr(separator + 2) };
}

void ValidateProject(const Project& project) {
	PTGN_ASSERT(!project.asset_directory.empty(), "Project asset directory cannot be empty");

	PTGN_ASSERT(
		!project.asset_directory.is_absolute() &&
			!project.asset_directory.lexically_normal().generic_string().starts_with(".."),
		"Project asset directory must remain inside the project root: ",
		project.asset_directory.string()
	);

	if (project.scenes.empty()) {
		PTGN_ASSERT(
			project.startup_scene_key.empty(), "Project with no scenes cannot have a startup scene"
		);
	} else {
		PTGN_ASSERT(
			!project.startup_scene_key.empty(), "Project with scenes must have a startup scene"
		);

		PTGN_ASSERT(
			FindProjectScene(project, project.startup_scene_key),
			"Project startup scene key is not present in the scene list: ",
			project.startup_scene_key
		);
	}

	for (std::size_t i{ 0 }; i < project.scenes.size(); ++i) {
		const auto& scene{ project.scenes[i] };

		PTGN_ASSERT(!scene.key.empty(), "Project scene key cannot be empty");

		PTGN_ASSERT(!scene.display_name.empty(), "Project scene display name cannot be empty");

		PTGN_ASSERT(!scene.scene_path.empty(), "Project scene path cannot be empty");

		for (std::size_t j{ i + 1 }; j < project.scenes.size(); ++j) {
			const auto& other{ project.scenes[j] };

			PTGN_ASSERT(scene.key != other.key, "Duplicate project scene key: ", scene.key);

			PTGN_ASSERT(
				!ScenePathsEqual(scene.scene_path, other.scene_path),
				"Duplicate project scene path: ", scene.scene_path.string()
			);
		}
	}
}

} // namespace

namespace impl {

bool SaveProjectScenes(Application& app, std::span<const Scene* const> scenes) {
	auto& app_context{ ApplicationAccessor::ctx(app) };
	if (!app_context.project.has_value()) {
		return false;
	}

	auto& project{ app_context.project.value() };

	struct PendingSceneWrite {
		path file_path{};
		SerializedScene scene{};
	};

	std::vector<PendingSceneWrite> writes;
	writes.reserve(scenes.size());

	for (const Scene* scene : scenes) {
		if (!scene || scene->IsRuntime()) {
			return false;
		}

		const auto* project_scene{ FindProjectScene(project, scene->GetTag()) };
		PTGN_ASSERT(project_scene, "Scene is not registered in project: ", scene->GetTag());

		writes.emplace_back(
			PendingSceneWrite{
				.file_path = GetProjectScenePath(project, *project_scene),
				.scene	   = CaptureScene(*scene),
			}
		);
	}

	project.assets		   = app_context.assets.GetCatalog();
	project.preload_assets = app_context.assets.GetProjectAssetDependencies();
	project.settings	   = GetProjectSettings(app);
	SaveProject(project);

	for (const auto& write : writes) {
		SaveSceneFile(write.file_path, write.scene);
	}
	return true;
}

bool SaveBootstrapProjectScene(Application& app, const Scene& scene, bool save) {
	if (!save || scene.IsRuntime()) {
		return false;
	}

	const Scene* scenes[]{ &scene };
	if (!SaveProjectScenes(app, std::span<const Scene* const>{ scenes })) {
		return false;
	}

	ApplicationAccessor::ctx(app).project_bootstrap_save_pending = false;
	return true;
}

} // namespace impl

Project LoadProject(const path& file_path, const ProjectSettings& default_settings) {
	Project project;
	project.settings = default_settings;

	LoadJson(file_path).get_to(project);
	project.file_path = file_path;

	ValidateProject(project);
	EnsureProjectAssetDirectories(project);
	return project;
}

Project CreateProject(
	const path& file_path, const impl::SceneRegistryEntry* default_scene, ProjectSettings settings
) {
	Project project{
		.name			 = file_path.stem().string(),
		.file_path		 = file_path,
		.asset_directory = "assets",
		.settings		 = std::move(settings),
	};

	EnsureProjectAssetDirectories(project);

	if (default_scene) {
		project.startup_scene_key = "main";

		project.scenes.emplace_back(
			ProjectSceneEntry{
				.key		  = "main",
				.display_name = TypeNameWithoutNamespaces(default_scene->type),
				.scene_path	  = path{ "assets" } / "scenes" / "main.ptgnscene",
			}
		);

		SaveSceneFile(
			GetStartupScenePath(project), SerializedScene{
											  .type			  = default_scene->type,
											  .parameters	  = default_scene->default_parameters(),
											  .assets		  = {},
											  .preload_assets = {},
											  .content		  = std::nullopt,
										  }
		);
	}

	SaveProject(project);

	return project;
}

void SaveProject(const Project& project) {
	ValidateProject(project);
	EnsureProjectAssetDirectories(project);
	EnsureDirectory(project.file_path.parent_path());
	json value = project;
	SaveJson(value, project.file_path);
}

ProjectSceneEntry* FindProjectScene(Project& project, std::string_view scene_key) {
	auto it{ std::ranges::find_if(project.scenes, [scene_key](const ProjectSceneEntry& scene) {
		return scene.key == scene_key;
	}) };
	return it == project.scenes.end() ? nullptr : &*it;
}

const ProjectSceneEntry* FindProjectScene(const Project& project, std::string_view scene_key) {
	auto it{ std::ranges::find_if(project.scenes, [scene_key](const ProjectSceneEntry& scene) {
		return scene.key == scene_key;
	}) };
	return it == project.scenes.end() ? nullptr : &*it;
}

const ProjectSceneEntry& GetStartupProjectScene(const Project& project) {
	const auto* scene{ FindProjectScene(project, project.startup_scene_key) };
	PTGN_ASSERT(scene, "Project startup scene key is missing: ", project.startup_scene_key);
	return *scene;
}

path GetProjectScenePath(const Project& project, const ProjectSceneEntry& scene) {
	return project.file_path.parent_path() / scene.scene_path;
}

path GetStartupScenePath(const Project& project) {
	return GetProjectScenePath(project, GetStartupProjectScene(project));
}

path GetProjectAssetDirectory(const Project& project) {
	return project.file_path.parent_path() / project.asset_directory;
}

void EnsureProjectAssetDirectories(const Project& project) {
	auto root{ GetProjectAssetDirectory(project) };
	EnsureDirectory(root);

	constexpr std::array<std::string_view, 7> directories{
		"audio", "data", "fonts", "prefabs", "scenes", "shaders", "textures",
	};

	for (auto directory : directories) {
		EnsureDirectory(root / directory);
	}
}

} // namespace ptgn
