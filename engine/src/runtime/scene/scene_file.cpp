#include "runtime/scene/scene_file.h"

#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "app/application_context.h"
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
		"Serialized scene parameters must be an object. Found ",
		scene.parameters.type_name(),
		": ",
		scene.parameters.dump(2)
	);

	PTGN_ASSERT(
		!scene.content.has_value() ||
			scene.content->is_object(),
		"Serialized scene content must be an object or absent. Found ",
		scene.content.has_value()
			? scene.content->type_name()
			: "absent",
		scene.content.has_value()
			? ": " + scene.content->dump(2)
			: ""
	);

	for (const auto& key : scene.assets) {
		PTGN_ASSERT(
			!key.value.empty(),
			"Serialized scene asset key cannot be empty"
		);
	}

	for (const auto& key : scene.preload_assets) {
		PTGN_ASSERT(
			!key.value.empty(),
			"Serialized scene preload asset key cannot be empty"
		);
		PTGN_ASSERT(
			std::ranges::contains(scene.assets, key),
			"Serialized scene preload asset must also be present in assets: ",
			key
		);
	}
}

[[nodiscard]] SerializedScene FromJson(const json& value) {
	PTGN_ASSERT(
		value.is_object(),
		"Serialized scene file root must be an object. Found ",
		value.type_name(),
		": ",
		value.dump(2)
	);

	PTGN_ASSERT(
		value.contains("type"),
		"Serialized scene file is missing type"
	);

	PTGN_ASSERT(
		value.contains("parameters"),
		"Serialized scene file is missing parameters"
	);

	PTGN_ASSERT(
		value.contains("assets"),
		"Serialized scene file is missing assets"
	);

	PTGN_ASSERT(
		value.contains("preload_assets"),
		"Serialized scene file is missing preload_assets"
	);

	PTGN_ASSERT(
		value.contains("content"),
		"Serialized scene file is missing content"
	);

	const auto& type_value{ value.at("type") };
	const auto& parameters_value{ value.at("parameters") };
	const auto& assets_value{ value.at("assets") };
	const auto& preload_assets_value{ value.at("preload_assets") };
	const auto& content_value{ value.at("content") };

	PTGN_ASSERT(
		type_value.is_string(),
		"Serialized scene type must be a string. Found ",
		type_value.type_name(),
		": ",
		type_value.dump(2)
	);

	PTGN_ASSERT(
		parameters_value.is_object(),
		"Serialized scene parameters must be an object. Found ",
		parameters_value.type_name(),
		": ",
		parameters_value.dump(2)
	);

	PTGN_ASSERT(
		assets_value.is_array(),
		"Serialized scene assets must be an array. Found ",
		assets_value.type_name(),
		": ",
		assets_value.dump(2)
	);

	PTGN_ASSERT(
		preload_assets_value.is_array(),
		"Serialized scene preload_assets must be an array. Found ",
		preload_assets_value.type_name(),
		": ",
		preload_assets_value.dump(2)
	);

	PTGN_ASSERT(
		content_value.is_null() ||
			content_value.is_object(),
		"Serialized scene content must be an object or null. Found ",
		content_value.type_name(),
		": ",
		content_value.dump(2)
	);

	SerializedScene scene;

	type_value.get_to(scene.type);
	scene.parameters = parameters_value;
	assets_value.get_to(scene.assets);
	preload_assets_value.get_to(scene.preload_assets);

	if (content_value.is_object()) {
		scene.content = content_value;
	}

	ValidateSerializedScene(scene);

	return scene;
}

[[nodiscard]] json ToJson(const SerializedScene& scene) {
	ValidateSerializedScene(scene);

	json value = json::object();

	value["type"] = scene.type;
	value["parameters"] = scene.parameters;
	value["assets"] = scene.assets;
	value["preload_assets"] = scene.preload_assets;

	if (scene.content.has_value()) {
		value["content"] = scene.content.value();
	} else {
		value["content"] = nullptr;
	}

	return value;
}

void AddUnique(
	std::vector<AssetKey>& values,
	const AssetKey& key
) {
	if (!key.value.empty() &&
		!std::ranges::contains(values, key)) {
		values.emplace_back(key);
	}
}

} // namespace

SerializedScene LoadSceneFile(const path& file_path) {
	const json value = LoadJson(file_path);

	return FromJson(value);
}

void SaveSceneFile(
	const path& file_path,
	const SerializedScene& scene
) {
	const json value = ToJson(scene);

	EnsureDirectory(file_path.parent_path());
	SaveJson(value, file_path);
}

namespace impl {

std::vector<AssetKey> DiscoverSceneAssetDependencies(
	const Scene& scene,
	std::span<const AssetKey> explicit_dependencies
) {
	json parameters = json::object();

	if (!scene.GetRegisteredType().empty()) {
		const auto& registration{
			GetSceneRegistration(scene.GetRegisteredType())
		};
		parameters = registration.serialize_parameters(scene);
	}

	json content = scene.SerializeContent();
	json dependency_source = json::object();
	dependency_source["parameters"] = std::move(parameters);
	dependency_source["content"] = std::move(content);

	return scene.ctx().asset.DiscoverDependencies(
		dependency_source,
		explicit_dependencies
	);
}

} // namespace impl

SerializedScene CaptureScene(const Scene& scene) {
	SerializedScene captured;

	if (scene.GetRegisteredType().empty()) {
		captured.type = std::string{
			impl::kBaseSceneType
		};
		captured.parameters = json::object();
	} else {
		const auto& registration{
			impl::GetSceneRegistration(
				scene.GetRegisteredType()
			)
		};

		captured.type = registration.type;
		captured.parameters =
			registration.serialize_parameters(scene);
	}

	PTGN_ASSERT(
		captured.parameters.is_object(),
		"Serialized scene parameters must be an object. Found ",
		captured.parameters.type_name(),
		": ",
		captured.parameters.dump(2)
	);

	json content = scene.SerializeContent();

	PTGN_ASSERT(
		content.is_object(),
		"Scene::SerializeContent() must return an object. Found ",
		content.type_name(),
		": ",
		content.dump(2)
	);

	captured.preload_assets = scene.GetExplicitAssetDependencies();
	captured.assets = impl::DiscoverSceneAssetDependencies(
		scene,
		captured.preload_assets
	);

	captured.content = std::move(content);

	ValidateSerializedScene(captured);

	return captured;
}

namespace impl {

class SceneFileAccess {
public:
	static void SetAssetDependencies(
		Scene& scene,
		std::vector<AssetKey> dependencies,
		std::vector<AssetKey> explicit_dependencies
	) {
		scene.SetAssetDependencies(
			std::move(dependencies),
			std::move(explicit_dependencies)
		);
	}

	static void InitNew(
		Scene& scene,
		Application& app,
		SceneData&& scene_data
	) {
		scene.Init(
			app,
			std::move(scene_data)
		);
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
	SerializedScene scene,
	bool runtime
) {
	auto serialized =
		std::make_shared<const SerializedScene>(
			std::move(scene)
		);

	SceneFactory::Construct construct =
		[serialized, runtime](
			Application& app,
			SceneData&& scene_data
		) -> std::unique_ptr<Scene> {
		std::unique_ptr<Scene> output;

		if (serialized->type ==
			kBaseSceneType) {
			output =
				std::make_unique<Scene>();

			scene_data.registered_type.clear();
		} else {
			const auto& registration{
				GetSceneRegistration(
					serialized->type
				)
			};

			output = registration.construct(
				serialized->parameters
			);

			PTGN_ASSERT(
				output,
				"Registered scene factory returned null: ",
				serialized->type
			);

			scene_data.registered_type =
				serialized->type;
		}

		scene_data.runtime = runtime;

		SceneFileAccess::SetAssetDependencies(
			*output,
			serialized->assets,
			serialized->preload_assets
		);

		if (serialized->content.has_value()) {
			SceneFileAccess::InitLoaded(
				*output,
				app,
				std::move(scene_data),
				serialized->content.value()
			);
		} else {
			SceneFileAccess::InitNew(
				*output,
				app,
				std::move(scene_data)
			);
		}

		return output;
	};

	SceneFactory::Preload preload =
		[serialized](Application& app) {
		std::vector<AssetKey> dependencies =
			serialized->assets;

		if (serialized->type !=
			kBaseSceneType) {
			const auto& registration{
				GetSceneRegistration(
					serialized->type
				)
			};

			for (const auto& key :
				 registration.preload_dependencies(
					 serialized->parameters
				 )) {
				AddUnique(
					dependencies,
					key
				);
			}
		}

		json parameter_source =
			serialized->parameters;

		return ApplicationAccessor::ctx(app)
			.assets.DiscoverDependencies(
				parameter_source,
				dependencies
			);
	};

	return SceneFactory{
		std::move(construct),
		std::move(preload)
	};
}

} // namespace impl

} // namespace ptgn
