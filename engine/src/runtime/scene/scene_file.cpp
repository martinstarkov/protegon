#include "runtime/scene/scene_file.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "core/assert.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_registry.h"

namespace ptgn {

namespace {

[[nodiscard]] json ToJson(const SerializedScene& scene) {
	return json{
		{ "type", scene.type },
		{ "parameters", scene.parameters },
		{ "content", scene.content.has_value() ? scene.content.value() : json{} },
	};
}

[[nodiscard]] SerializedScene FromJson(const json& value) {
	SerializedScene scene{
		.type = value.at("type").get<std::string>(),
		.parameters = value.value("parameters", json::object()),
	};

	if (const auto it{ value.find("content") }; it != value.end() && !it->is_null()) {
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
	PTGN_ASSERT(
		!scene.GetRegisteredType().empty(),
		"Cannot serialize a project scene without a registered scene type"
	);

	const auto& registration{ impl::GetSceneRegistration(scene.GetRegisteredType()) };

	return SerializedScene{
		.type = std::string{ scene.GetRegisteredType() },
		.parameters = registration.serialize_parameters(scene),
		.content = scene.SerializeContent(),
	};
}

namespace impl {

class SceneFileAccess {
public:
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
			   Application& app, SceneData&& scene_data
		   ) mutable -> std::unique_ptr<Scene> {
		const auto& registration{ GetSceneRegistration(serialized.type) };
		auto scene{ registration.construct(serialized.parameters) };
		PTGN_ASSERT(scene, "Registered scene factory returned null: ", serialized.type);

		scene_data.runtime = runtime;
		scene_data.registered_type = serialized.type;

		if (serialized.content.has_value()) {
			SceneFileAccess::InitLoaded(
				*scene, app, std::move(scene_data), serialized.content.value()
			);
		} else {
			SceneFileAccess::InitNew(*scene, app, std::move(scene_data));
		}

		return scene;
	};
}

} // namespace impl

} // namespace ptgn
