#include "runtime/scene/scene_file.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/application_context.h"
#include "app/project.h"
#include "core/assert.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

void ValidateSerializedScene(const SerializedScene& scene) {
	PTGN_ASSERT(
		!scene.type.empty(),
		"Serialized scene type cannot be empty"
	);

	PTGN_ASSERT(
		scene.parameters.is_object(),
		"Serialized scene parameters must be a JSON object"
	);

	PTGN_ASSERT(
		!scene.content.has_value() ||
			scene.content->is_object(),
		"Serialized scene content must be an object or null"
	);

	for (const auto& key : scene.assets) {
		PTGN_ASSERT(
			!key.value.empty(),
			"Serialized scene asset key cannot be empty"
		);
	}
}

} // namespace

SerializedScene LoadSceneFile(const path& file_path) {
	auto scene{ LoadJson(file_path).get<SerializedScene>() };

	ValidateSerializedScene(scene);

	return scene;
}

void SaveSceneFile(
	const path& path,
	const SerializedScene& scene
) {
	ValidateSerializedScene(scene);

	EnsureDirectory(path.parent_path());

	json value = scene;

	SaveJson(value, path);
}

SerializedScene CaptureScene(const Scene& scene) {
	std::string type;
	json parameters = json::object();

	if (scene.GetRegisteredType().empty()) {
		type = std::string{ impl::kBaseSceneType };
	} else {
		const auto& registration{
			impl::GetSceneRegistration(
				scene.GetRegisteredType()
			)
		};

		type = registration.type;
		parameters =
			registration.serialize_parameters(scene);
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
	static void SetAssetDependencies(
		Scene& scene,
		std::vector<AssetKey> dependencies
	) {
		scene.asset_dependencies_ = std::move(dependencies);
	}

	static void InitNew(
		Scene& scene,
		Application& app,
		SceneData&& scene_data
	) {
		scene.Init(app, std::move(scene_data));
	}

	static void InitLoaded(
		Scene& scene,
		Application& app,
		SceneData&& scene_data,
		const json& content
	) {
		scene.Init(
			app,
			std::move(scene_data),
			content
		);
	}
};

SceneFactory MakeSceneFactory(
	SerializedScene serialized,
	bool runtime
) {
	return [
		serialized = std::move(serialized),
		runtime
	](
		Application& app,
		SceneData&& scene_data
	) mutable -> std::unique_ptr<Scene> {
		auto& app_context{
			ApplicationAccessor::ctx(app)
		};

		if (app_context.project.has_value()) {
			const auto& project{
				app_context.project.value()
			};

			app_context.assets.RegisterCatalog(
				project.assets
			);

			app_context.assets.AddProjectAssetDependencies(
				project.preload_assets
			);

			app_context.assets.LoadDependencies(
				project.preload_assets
			);
		}

		app_context.assets.LoadDependencies(
			serialized.assets
		);

		std::unique_ptr<Scene> scene;

		if (serialized.type == kBaseSceneType) {
			scene = std::make_unique<Scene>();
			scene_data.registered_type.clear();
		} else {
			const auto& registration{
				GetSceneRegistration(serialized.type)
			};

			scene = registration.construct(
				serialized.parameters
			);

			PTGN_ASSERT(
				scene,
				"Registered scene factory returned null: ",
				serialized.type
			);

			scene_data.registered_type =
				serialized.type;
		}

		scene_data.runtime = runtime;

		SceneFileAccess::SetAssetDependencies(
			*scene,
			serialized.assets
		);

		if (serialized.content.has_value()) {
			SceneFileAccess::InitLoaded(
				*scene,
				app,
				std::move(scene_data),
				serialized.content.value()
			);
		} else {
			SceneFileAccess::InitNew(
				*scene,
				app,
				std::move(scene_data)
			);
		}

		return scene;
	};
}

} // namespace impl

} // namespace ptgn
