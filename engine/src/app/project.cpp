#include "app/project.h"

#include <fstream>
#include <string>

#include "core/assert.h"
#include "core/util/file.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"

namespace ptgn {

Project LoadProject(const path& filepath) {
	std::ifstream stream{ filepath };
	PTGN_ASSERT(stream.is_open(), "Failed to open project file: ", filepath.string());

	json value;
	stream >> value;

	return Project{
		.name = value.at("name").get<std::string>(),
		.file_path = filepath,
		.startup_scene = path{ value.at("startup_scene").get<std::string>() },
	};
}

Project CreateProject(
	const path& filepath, const impl::SceneRegistryEntry& default_scene
) {
	const auto root{ filepath.parent_path() };
	if (!root.empty()) {
		std::filesystem::create_directories(root);
	}
	std::filesystem::create_directories(root / "Scenes");

	Project project{
		.name = filepath.stem().string(),
		.file_path = filepath,
		.startup_scene = path{ "Scenes" } / "Main.ptgnscene",
	};

	SaveProject(project);

	SaveSceneFile(
		GetStartupScenePath(project),
		SerializedScene{
			.type = default_scene.type,
			.parameters = default_scene.default_parameters(),
			.content = std::nullopt,
		}
	);

	return project;
}

void SaveProject(const Project& project) {
	if (const auto parent{ project.file_path.parent_path() }; !parent.empty()) {
		std::filesystem::create_directories(parent);
	}

	std::ofstream stream{ project.file_path, std::ios::trunc };
	PTGN_ASSERT(stream.is_open(), "Failed to write project file: ", project.file_path.string());

	json value{
		{ "name", project.name },
		{ "startup_scene", project.startup_scene.generic_string() },
	};
	stream << value.dump(4) << '\n';
}

path GetStartupScenePath(const Project& project) {
	return project.file_path.parent_path() / project.startup_scene;
}

} // namespace ptgn
