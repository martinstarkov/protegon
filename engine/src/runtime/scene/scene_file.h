#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

namespace impl {

inline constexpr std::string_view kBaseSceneType{ "$Scene" };

} // namespace impl

struct SerializedScene {
	std::string type;
	json parameters = json::object();
	std::vector<AssetKey> assets;

	/// @brief Nullopt for unserialized scenes.
	std::optional<json> content;

	PTGN_REFLECT(
		SerializedScene,
		type,
		parameters,
		assets,
		content
	)
};

[[nodiscard]] SerializedScene LoadSceneFile(const path& file_path);

void SaveSceneFile(
	const path& file_path,
	const SerializedScene& scene
);

[[nodiscard]] SerializedScene CaptureScene(const Scene& scene);

namespace impl {

[[nodiscard]] SceneFactory MakeSceneFactory(
	SerializedScene scene,
	bool runtime
);

} // namespace impl

} // namespace ptgn