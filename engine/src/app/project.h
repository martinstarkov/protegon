#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_serialization.h"

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

/// @brief Performs the one-time automatic save after a new scene's OnNew().
bool SaveBootstrapProjectScene(
	Application& app,
	const Scene& scene,
	bool save
);

} // namespace impl

struct ProjectSceneEntry {
	std::string tag;
	std::filesystem::path scene_path;
};

struct Project {
	std::string name;
	std::filesystem::path file_path;

	/// @brief Project-relative path of the scene used for runtime startup.
	std::filesystem::path startup_scene;

	/// @brief Every serialized scene belonging to the project.
	std::vector<ProjectSceneEntry> scenes;

	/// @brief Complete path-backed asset catalog known to the project.
	std::vector<SerializedAsset> assets;

	/// @brief Assets loaded globally whenever the project is opened.
	std::vector<AssetKey> preload_assets;
};

[[nodiscard]] Project LoadProject(const std::filesystem::path& path);

[[nodiscard]] Project CreateProject(
	const std::filesystem::path& path,
	const impl::SceneRegistryEntry& default_scene
);

void SaveProject(const Project& project);

[[nodiscard]] ProjectSceneEntry* FindProjectScene(
	Project& project,
	std::string_view scene_tag
);

[[nodiscard]] const ProjectSceneEntry* FindProjectScene(
	const Project& project,
	std::string_view scene_tag
);

[[nodiscard]] const ProjectSceneEntry& GetStartupProjectScene(
	const Project& project
);

[[nodiscard]] std::filesystem::path GetProjectScenePath(
	const Project& project,
	const ProjectSceneEntry& scene
);

[[nodiscard]] std::filesystem::path GetStartupScenePath(
	const Project& project
);

} // namespace ptgn