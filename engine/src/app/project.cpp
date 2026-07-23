#include "app/project.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <span>
#include <vector>

#include "core/util/file.h"
#include "core/assert.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"
#include "app/application_context.h"
#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace {

[[nodiscard]] bool ScenePathsEqual(
	const path& a,
	const path& b
) {
	return a.lexically_normal().generic_string() ==
		   b.lexically_normal().generic_string();
}

void ValidateProject(const Project& project) {
	PTGN_ASSERT(
		!project.startup_scene.empty(),
		"Project startup scene path cannot be empty"
	);

	PTGN_ASSERT(
		!project.scenes.empty(),
		"Project must contain at least one scene"
	);

	for (std::size_t i{ 0 }; i < project.scenes.size(); ++i) {
		const auto& scene{ project.scenes[i] };

		PTGN_ASSERT(
			!scene.tag.empty(),
			"Project scene tag cannot be empty"
		);

		PTGN_ASSERT(
			!scene.scene_path.empty(),
			"Project scene path cannot be empty: ",
			scene.tag
		);

		for (std::size_t j{ i + 1 }; j < project.scenes.size(); ++j) {
			const auto& other{ project.scenes[j] };

			PTGN_ASSERT(
				scene.tag != other.tag,
				"Duplicate project scene tag: ",
				scene.tag
			);

			PTGN_ASSERT(
				!ScenePathsEqual(scene.scene_path, other.scene_path),
				"Duplicate project scene path: ",
				scene.scene_path.string()
			);
		}
	}

	bool startup_scene_exists{ std::ranges::any_of(
		project.scenes,
		[&project](const ProjectSceneEntry& scene) {
			return ScenePathsEqual(
				scene.scene_path,
				project.startup_scene
			);
		}
	) };

	PTGN_ASSERT(
		startup_scene_exists,
		"Project startup scene is not present in the project scene list: ",
		project.startup_scene.string()
	);
}

[[nodiscard]] json SerializeAsset(const SerializedAsset& asset) {
	json value = json::object();
	value["key"] = asset.key.value;
	value["kind"] = asset.kind;
	value["path"] = asset.source_path.generic_string();
	return value;
}

[[nodiscard]] SerializedAsset DeserializeAsset(const json& value) {
	PTGN_ASSERT(value.is_object(), "Serialized project asset must be a JSON object");

	SerializedAsset asset{
		.key = AssetKey{ value.at("key").get<std::string>() },
		.kind = value.at("kind").get<AssetKind>(),
		.source_path = path{ value.at("path").get<std::string>() },
	};

	PTGN_ASSERT(!asset.key.value.empty(), "Serialized project asset key cannot be empty");
	PTGN_ASSERT(
		asset.kind != AssetKind::Unknown,
		"Serialized project asset kind cannot be Unknown: ",
		asset.key
	);
	PTGN_ASSERT(
		!asset.source_path.empty(),
		"Serialized project asset path cannot be empty: ",
		asset.key
	);

	return asset;
}

} // namespace

namespace impl {

bool SaveProjectScenes(
	Application& app,
	std::span<const Scene* const> scenes
) {
	auto& app_context{ ApplicationAccessor::ctx(app) };

	if (!app_context.project.has_value()) {
		return false;
	}

	auto& project{ app_context.project.value() };

	struct PendingSceneWrite {
		path file_path;
		SerializedScene scene;
	};

	std::vector<PendingSceneWrite> writes;
	writes.reserve(scenes.size());

	// Capture and validate every scene before modifying any files.
	for (const Scene* scene : scenes) {
		if (!scene || scene->IsRuntime()) {
			return false;
		}

		const auto* project_scene{
			FindProjectScene(project, scene->GetTag())
		};

		PTGN_ASSERT(
			project_scene,
			"Scene is not registered in the project manifest: ",
			scene->GetTag()
		);

		writes.emplace_back(PendingSceneWrite{
			.file_path = GetProjectScenePath(
				project,
				*project_scene
			),
			.scene = CaptureScene(*scene),
		});
	}

	project.assets = app_context.assets.GetCatalog();
	project.preload_assets =
		app_context.assets.GetProjectAssetDependencies();

	// Catalog descriptors must exist before scene dependency keys
	// are written to their scene files.
	SaveProject(project);

	for (const auto& write : writes) {
		SaveSceneFile(
			write.file_path,
			write.scene
		);
	}

	return true;
}

bool SaveBootstrapProjectScene(
	Application& app,
	const Scene& scene,
	bool save
) {
	if (!save || scene.IsRuntime()) {
		return false;
	}

	const Scene* scenes[]{ &scene };

	if (!SaveProjectScenes(
			app,
			std::span<const Scene* const>{ scenes }
		)) {
		return false;
	}

	ApplicationAccessor::ctx(app)
		.project_bootstrap_save_pending = false;

	return true;
}

} // namespace impl

Project LoadProject(const std::filesystem::path& path) {
	std::ifstream stream{ path };

	PTGN_ASSERT(
		stream.is_open(),
		"Failed to open project file: ",
		path.string()
	);

	json value;
	stream >> value;

	PTGN_ASSERT(
		value.is_object(),
		"Project file root must be a JSON object"
	);

	Project project{
		.name = value.at("name").get<std::string>(),
		.file_path = path,
		.startup_scene = std::filesystem::path{
			value.at("startup_scene").get<std::string>()
		},
	};

	if (const auto it{ value.find("scenes") }; it != value.end()) {
		PTGN_ASSERT(
			it->is_array(),
			"Project scenes must be a JSON array"
		);

		for (const auto& serialized_scene : *it) {
			PTGN_ASSERT(
				serialized_scene.is_object(),
				"Project scene entry must be a JSON object"
			);

			project.scenes.emplace_back(ProjectSceneEntry{
				.tag = serialized_scene.at("tag").get<std::string>(),
				.scene_path = std::filesystem::path{
					serialized_scene.at("path").get<std::string>()
				},
			});
		}
	}

	// Backward compatibility for the original one-scene format.
	if (project.scenes.empty()) {
		project.scenes.emplace_back(ProjectSceneEntry{
			.tag = "Main",
			.scene_path = project.startup_scene,
		});
	}

	if (const auto it{ value.find("assets") }; it != value.end()) {
		PTGN_ASSERT(
			it->is_array(),
			"Project assets must be a JSON array"
		);

		for (const auto& serialized_asset : *it) {
			project.assets.emplace_back(
				DeserializeAsset(serialized_asset)
			);
		}
	}

	if (const auto it{ value.find("preload_assets") };
		it != value.end()) {
		PTGN_ASSERT(
			it->is_array(),
			"Project preload_assets must be a JSON array"
		);

		for (const auto& key : *it) {
			PTGN_ASSERT(
				key.is_string(),
				"Project preload asset keys must be strings"
			);

			project.preload_assets.emplace_back(
				key.get<std::string>()
			);
		}
	}

	ValidateProject(project);

	return project;
}

Project CreateProject(
	const std::filesystem::path& path,
	const impl::SceneRegistryEntry& default_scene
) {
	const auto root{ path.parent_path() };

	if (!root.empty()) {
		std::filesystem::create_directories(root);
	}

	std::filesystem::create_directories(root / "Scenes");

	Project project{
		.name = path.stem().string(),
		.file_path = path,
		.startup_scene =
			std::filesystem::path{ "Scenes" } / "Main.ptgnscene",
		.scenes = {
			ProjectSceneEntry{
				.tag = "Main",
				.scene_path =
					std::filesystem::path{ "Scenes" } /
					"Main.ptgnscene",
			},
		},
	};

	// Write the scene before the manifest so the project never points
	// at a scene file which does not exist.
	SaveSceneFile(
		GetStartupScenePath(project),
		SerializedScene{
			.type = default_scene.type,
			.parameters = default_scene.default_parameters(),
			.assets = {},
			.content = std::nullopt,
		}
	);

	SaveProject(project);

	return project;
}

void SaveProject(const Project& project) {
	ValidateProject(project);

	if (const auto parent{ project.file_path.parent_path() };
		!parent.empty()) {
		std::filesystem::create_directories(parent);
	}

	std::ofstream stream{
		project.file_path,
		std::ios::trunc
	};

	PTGN_ASSERT(
		stream.is_open(),
		"Failed to write project file: ",
		project.file_path.string()
	);

	json scenes = json::array();

	for (const auto& scene : project.scenes) {
		scenes.emplace_back(json{
			{ "tag", scene.tag },
			{ "path", scene.scene_path.generic_string() },
		});
	}

	json assets = json::array();

	for (const auto& asset : project.assets) {
		assets.emplace_back(SerializeAsset(asset));
	}

	json preload_assets = json::array();

	for (const auto& key : project.preload_assets) {
		preload_assets.emplace_back(key.value);
	}

	json value = json::object();

	value["name"] = project.name;
	value["startup_scene"] =
		project.startup_scene.generic_string();
	value["scenes"] = std::move(scenes);
	value["assets"] = std::move(assets);
	value["preload_assets"] = std::move(preload_assets);

	stream << value.dump(4) << '\n';
}

ProjectSceneEntry* FindProjectScene(
	Project& project,
	std::string_view scene_tag
) {
	auto it{ std::ranges::find_if(
		project.scenes,
		[scene_tag](const ProjectSceneEntry& scene) {
			return scene.tag == scene_tag;
		}
	) };

	return it == project.scenes.end()
		? nullptr
		: &*it;
}

const ProjectSceneEntry* FindProjectScene(
	const Project& project,
	std::string_view scene_tag
) {
	auto it{ std::ranges::find_if(
		project.scenes,
		[scene_tag](const ProjectSceneEntry& scene) {
			return scene.tag == scene_tag;
		}
	) };

	return it == project.scenes.end()
		? nullptr
		: &*it;
}

const ProjectSceneEntry& GetStartupProjectScene(
	const Project& project
) {
	auto it{ std::ranges::find_if(
		project.scenes,
		[&project](const ProjectSceneEntry& scene) {
			return ScenePathsEqual(
				scene.scene_path,
				project.startup_scene
			);
		}
	) };

	PTGN_ASSERT(
		it != project.scenes.end(),
		"Project startup scene is missing from scene list: ",
		project.startup_scene.string()
	);

	return *it;
}

std::filesystem::path GetProjectScenePath(
	const Project& project,
	const ProjectSceneEntry& scene
) {
	return project.file_path.parent_path() / scene.scene_path;
}

std::filesystem::path GetStartupScenePath(
	const Project& project
) {
	return GetProjectScenePath(
		project,
		GetStartupProjectScene(project)
	);
}

} // namespace ptgn
