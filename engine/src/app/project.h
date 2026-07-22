#pragma once

#include <string>

#include "core/util/file.h"

namespace ptgn {

namespace impl {

struct SceneRegistryEntry;

} // namespace impl

struct Project {
	std::string name;
	path file_path;
	path startup_scene;
};

[[nodiscard]] Project LoadProject(const path& path);
[[nodiscard]] Project CreateProject(
	const path& path, const impl::SceneRegistryEntry& default_scene
);
void SaveProject(const Project& project);

[[nodiscard]] path GetStartupScenePath(const Project& project);

} // namespace ptgn
