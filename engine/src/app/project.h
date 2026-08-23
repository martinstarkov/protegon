#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/project_settings.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_serialization.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
#include "serialization/serialize.h"

namespace ptgn {

class Application;
class Scene;

namespace impl {

struct SceneRegistryEntry;

bool SaveProjectScenes(Application& app, std::span<const Scene* const> scenes);

bool SaveBootstrapProjectScene(Application& app, const Scene& scene, bool save);

} // namespace impl

struct ProjectSceneEntry {
	std::string key{};
	std::string display_name{};
	path scene_path{};

	PTGN_REFLECT(ProjectSceneEntry, key, display_name, scene_path)
};

struct Project {
	std::string name{};

	/// @brief Runtime-only location from which the project was loaded.
	path file_path;

	/// @brief Project-relative root shown by the Content Browser.
	path asset_directory{ "Assets" };

	std::string startup_scene_key{};
	std::vector<ProjectSceneEntry> scenes{};
	std::vector<SerializedAsset> assets{};
	std::vector<AssetKey> preload_assets{};
	ScreenEffectSettings screen_effects{};
	ProjectSettings settings{};

	PTGN_REFLECT(
		Project,
		name,
		asset_directory,
		startup_scene_key,
		scenes,
		assets,
		preload_assets,
		screen_effects,
		settings
	)
};

Project LoadProject(const path& file_path, const ProjectSettings& default_settings = {});

Project CreateProject(
	const path& file_path,
	const impl::SceneRegistryEntry& default_scene,
	ProjectSettings settings = {}
);

void SaveProject(const Project& project);

ProjectSceneEntry* FindProjectScene(Project& project, std::string_view scene_key);
const ProjectSceneEntry* FindProjectScene(const Project& project, std::string_view scene_key);

const ProjectSceneEntry& GetStartupProjectScene(const Project& project);

path GetProjectScenePath(const Project& project, const ProjectSceneEntry& scene);
path GetStartupScenePath(const Project& project);
path GetProjectAssetDirectory(const Project& project);

/// @brief Creates the standard project folders without deleting or moving existing files.
void EnsureProjectAssetDirectories(const Project& project);

} // namespace ptgn
