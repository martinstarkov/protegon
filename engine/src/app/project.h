#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_serialization.h"

namespace ptgn {

class Application;
class Scene;

namespace impl {

struct SceneRegistryEntry;

bool SaveProjectScene(Application& app, const Scene& scene, bool save);

} // namespace impl

struct ProjectSceneEntry {
	std::string tag;
	std::filesystem::path path;
};

struct Project {
	std::string name;
	std::filesystem::path file_path;

	/// @brief Project-relative path of the scene launched in runtime mode.
	std::filesystem::path startup_scene;

	/// @brief Every serialized scene belonging to the project.
	std::vector<ProjectSceneEntry> scenes;

	std::vector<SerializedAsset> assets;
	std::vector<AssetKey> preload_assets;
};

[[nodiscard]] Project LoadProject(const std::filesystem::path& path);
[[nodiscard]] Project CreateProject(
	const std::filesystem::path& path, const impl::SceneRegistryEntry& default_scene
);
void SaveProject(const Project& project);

[[nodiscard]] std::filesystem::path GetStartupScenePath(const Project& project);

} // namespace ptgn
