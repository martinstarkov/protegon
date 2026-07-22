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

struct Project {
	std::string name;
	std::filesystem::path file_path;
	std::filesystem::path startup_scene;

	/// @brief Complete path-backed asset catalog known to the project.
	std::vector<SerializedAsset> assets;

	/// @brief Assets loaded globally whenever any project scene is constructed.
	std::vector<AssetKey> preload_assets;
};

[[nodiscard]] Project LoadProject(const std::filesystem::path& path);
[[nodiscard]] Project CreateProject(
	const std::filesystem::path& path, const impl::SceneRegistryEntry& default_scene
);
void SaveProject(const Project& project);

[[nodiscard]] std::filesystem::path GetStartupScenePath(const Project& project);

} // namespace ptgn
