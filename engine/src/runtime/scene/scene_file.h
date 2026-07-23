#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/asset/asset_key.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace impl {

inline constexpr std::string_view kBaseSceneType{ "$Scene" };

} // namespace impl

struct SerializedScene {
	std::string type;
	json parameters = json::object();
	std::vector<AssetKey> assets;
	std::optional<json> content;
};

[[nodiscard]] SerializedScene LoadSceneFile(
	const std::filesystem::path& path
);

void SaveSceneFile(
	const std::filesystem::path& path,
	const SerializedScene& scene
);

[[nodiscard]] SerializedScene CaptureScene(
	const Scene& scene
);

namespace impl {

[[nodiscard]] SceneFactory MakeSceneFactory(
	SerializedScene scene,
	bool runtime
);

} // namespace impl

} // namespace ptgn