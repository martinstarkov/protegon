#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/project_settings.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_serialization.h"
#include "serialization/serialize.h"

namespace ptgn {

class Application;
class Scene;

namespace impl {

struct SceneRegistryEntry;

/// @brief Saves project metadata and the specified persistent editor scenes.
/// All scenes are captured before any files are written.
bool SaveProjectScenes(
	Application& app,
	std::span<const Scene* const> scenes
);

/// @brief Performs the one time automatic save after a new scene's OnNew().
bool SaveBootstrapProjectScene(
	Application& app,
	const Scene& scene,
	bool save
);

} // namespace impl

struct ProjectSceneEntry {
	/// @brief Unique project scene identifier used by scene transitions and runtime scene tags.
	std::string key;

	/// @brief Non-unique editor-facing label.
	std::string display_name;

	path scene_path;

	PTGN_REFLECT(ProjectSceneEntry, key, display_name, scene_path)
};

struct Project {
	std::string name;

	/// @brief Absolute or working directory relative location from which this project was loaded.
	/// This is runtime state and is intentionally not serialized.
	path file_path;

	/// @brief Key of the project scene used for direct runtime startup.
	std::string startup_scene_key;

	/// @brief Every serialized scene belonging to the project, in editor/runtime ordering.
	std::vector<ProjectSceneEntry> scenes;

	/// @brief Complete path asset catalog known to the project.
	std::vector<SerializedAsset> assets;

	/// @brief Assets loaded globally whenever the project is opened.
	std::vector<AssetKey> preload_assets;

	/// @brief Shared, tracked project settings.
	ProjectSettings settings;

	PTGN_REFLECT(
		Project,
		name,
		startup_scene_key,
		scenes,
		assets,
		preload_assets,
		settings
	)
};

/// @brief Loads a project, using default_settings for fields missing from the manifest.
Project LoadProject(
	const path& file_path,
	const ProjectSettings& default_settings = {}
);

Project CreateProject(
	const path& file_path,
	const impl::SceneRegistryEntry& default_scene,
	ProjectSettings settings = {}
);

void SaveProject(const Project& project);

ProjectSceneEntry* FindProjectScene(
	Project& project,
	std::string_view scene_key
);

const ProjectSceneEntry* FindProjectScene(
	const Project& project,
	std::string_view scene_key
);

const ProjectSceneEntry& GetStartupProjectScene(
	const Project& project
);

path GetProjectScenePath(
	const Project& project,
	const ProjectSceneEntry& scene
);

path GetStartupScenePath(
	const Project& project
);

} // namespace ptgn
