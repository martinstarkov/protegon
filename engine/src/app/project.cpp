#include "app/project.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "core/assert.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"
#include "app/application_context.h"

namespace ptgn {

namespace {

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

bool SaveProjectScene(Application& app, const Scene& scene, bool save) {
	auto& app_context{ ApplicationAccessor::ctx(app) };

	if (!save ||
		!app_context.project.has_value() ||
		scene.IsRuntime() ||
		scene.GetRegisteredType().empty()) {
		return false;
	}

	auto& project{ app_context.project.value() };

	project.assets = app_context.assets.GetCatalog();
	project.preload_assets =
		app_context.assets.GetProjectAssetDependencies();

	// Save the catalog first. A scene dependency must never reach disk
	// before its catalog descriptor.
	SaveProject(project);

	SaveSceneFile(
		GetStartupScenePath(project),
		CaptureScene(scene)
	);

	app_context.project_bootstrap_save_pending = false;

	return true;
}

} // namespace impl

Project LoadProject(const std::filesystem::path& path) {
	std::ifstream stream{ path };
	PTGN_ASSERT(stream.is_open(), "Failed to open project file: ", path.string());

	json value;
	stream >> value;

	PTGN_ASSERT(value.is_object(), "Project file root must be a JSON object");

	Project project{
		.name = value.at("name").get<std::string>(),
		.file_path = path,
		.startup_scene = std::filesystem::path{ value.at("startup_scene").get<std::string>() },
	};

	if (const auto it{ value.find("assets") }; it != value.end()) {
		PTGN_ASSERT(it->is_array(), "Project assets must be a JSON array");

		for (const auto& serialized_asset : *it) {
			project.assets.emplace_back(DeserializeAsset(serialized_asset));
		}
	}

	if (const auto it{ value.find("preload_assets") }; it != value.end()) {
		PTGN_ASSERT(it->is_array(), "Project preload_assets must be a JSON array");

		for (const auto& key : *it) {
			PTGN_ASSERT(key.is_string(), "Project preload asset keys must be strings");
			project.preload_assets.emplace_back(key.get<std::string>());
		}
	}

	return project;
}

Project CreateProject(
	const std::filesystem::path& path, const impl::SceneRegistryEntry& default_scene
) {
	const auto root{ path.parent_path() };
	if (!root.empty()) {
		std::filesystem::create_directories(root);
	}
	std::filesystem::create_directories(root / "Scenes");

	Project project{
		.name = path.stem().string(),
		.file_path = path,
		.startup_scene = std::filesystem::path{ "Scenes" } / "Main.ptgnscene",
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
	value["startup_scene"] = project.startup_scene.generic_string();
	value["assets"] = std::move(assets);
	value["preload_assets"] = std::move(preload_assets);

	stream << value.dump(4) << '\n';

	PTGN_LOG("Saved project ", project.name);
}

std::filesystem::path GetStartupScenePath(const Project& project) {
	return project.file_path.parent_path() / project.startup_scene;
}

} // namespace ptgn
