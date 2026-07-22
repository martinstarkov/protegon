#include "runtime/scene/scene_file.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/assert.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"

namespace ptgn {

namespace {

[[nodiscard]] json ToJson(const SerializedScene& scene) {
	json assets = json::array();

	for (const auto& key : scene.assets) {
		assets.emplace_back(key.value);
	}

	json value = json::object();
	value["type"] = scene.type;
	value["parameters"] = scene.parameters;
	value["assets"] = std::move(assets);
	value["content"] = scene.content.has_value() ? scene.content.value() : json{};
	return value;
}

[[nodiscard]] SerializedScene FromJson(const json& value) {
	PTGN_ASSERT(value.is_object(), "Serialized scene file root must be a JSON object");

	SerializedScene scene{
		.type = value.at("type").get<std::string>(),
		.parameters = value.value("parameters", json::object()),
	};

	PTGN_ASSERT(scene.parameters.is_object(), "Serialized scene parameters must be a JSON object");

	if (const auto it{ value.find("assets") }; it != value.end()) {
		PTGN_ASSERT(it->is_array(), "Serialized scene assets must be a JSON array");

		for (const auto& key : *it) {
			PTGN_ASSERT(key.is_string(), "Serialized scene asset keys must be strings");
			scene.assets.emplace_back(key.get<std::string>());
		}
	}

	if (const auto it{ value.find("content") }; it != value.end() && !it->is_null()) {
		PTGN_ASSERT(it->is_object(), "Serialized scene content must be a JSON object or null");
		scene.content = *it;
	}

	return scene;
}

} // namespace

SerializedScene LoadSceneFile(const std::filesystem::path& path) {
	std::ifstream stream{ path };
	PTGN_ASSERT(stream.is_open(), "Failed to open scene file: ", path.string());

	json value;
	stream >> value;
	return FromJson(value);
}

void SaveSceneFile(const std::filesystem::path& path, const SerializedScene& scene) {
	if (const auto parent{ path.parent_path() }; !parent.empty()) {
		std::filesystem::create_directories(parent);
	}

	std::ofstream stream{ path, std::ios::trunc };
	PTGN_ASSERT(stream.is_open(), "Failed to write scene file: ", path.string());
	stream << ToJson(scene).dump(4) << '\n';
}

SerializedScene CaptureScene(const Scene& scene) {
	std::string type;
	json parameters = json::object();

	if (scene.GetRegisteredType().empty()) {
		type = impl::kBaseSceneType;
	} else {
		const auto& registration{
			impl::GetSceneRegistration(scene.GetRegisteredType())
		};

		type = scene.GetRegisteredType();
		parameters = registration.serialize_parameters(scene);
	}

	return SerializedScene{
		.type = std::move(type),
		.parameters = std::move(parameters),
		.assets = scene.GetAssetDependencies(),
		.content = scene.SerializeContent(),
	};
}

namespace impl {

class SceneFileAccess {
public:
	static void SetAssetDependencies(Scene& scene, std::vector<AssetKey> dependencies) {
		scene.asset_dependencies_ = std::move(dependencies);
	}

	static void InitNew(Scene& scene, Application& app, SceneData&& scene_data) {
		scene.Init(app, std::move(scene_data));
	}

	static void InitLoaded(
		Scene& scene, Application& app, SceneData&& scene_data, const json& content
	) {
		scene.Init(app, std::move(scene_data), content);
	}
};

SceneFactory MakeSceneFactory(SerializedScene serialized, bool runtime) {
	return [serialized = std::move(serialized), runtime](
			   Application& app,
			   SceneData&& scene_data
		   ) mutable -> std::unique_ptr<Scene> {
		auto& app_context{ ApplicationAccessor::ctx(app) };

		if (app_context.project.has_value()) {
			const auto& project{ app_context.project.value() };

			app_context.assets.RegisterCatalog(project.assets);
			app_context.assets.AddProjectAssetDependencies(project.preload_assets);
			app_context.assets.LoadDependencies(project.preload_assets);
		}

		scene_data.runtime = runtime;

		app_context.assets.LoadDependencies(serialized.assets);

		std::unique_ptr<Scene> scene;

		if (serialized.type == impl::kBaseSceneType) {
			scene = std::make_unique<Scene>();
			scene_data.registered_type.clear();
		} else {
			const auto& registration{ GetSceneRegistration(serialized.type) };

			scene = registration.construct(serialized.parameters);
			PTGN_ASSERT(
				scene,
				"Registered scene factory returned null: ",
				serialized.type
			);

			scene_data.registered_type = serialized.type;
		}

		SceneFileAccess::SetAssetDependencies(*scene, serialized.assets);

		if (serialized.content.has_value()) {
			SceneFileAccess::InitLoaded(
				*scene,
				app,
				std::move(scene_data),
				serialized.content.value()
			);
		} else {
			SceneFileAccess::InitNew(*scene, app, std::move(scene_data));
		}

		return scene;
	};
}

} // namespace impl

} // namespace ptgn
