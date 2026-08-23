#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace impl {

inline constexpr std::string_view kBaseSceneType{ "$Scene" };

} // namespace impl

struct SerializedScene {
	std::string type{};
	json parameters = json::object();
	/// @brief Complete effective dependency set used for preloading before scene construction.
	std::vector<AssetKey> assets{};

	/// @brief Explicit preload only dependencies added through Scene::AddAssetDependency or the editor.
	/// These remain distinct from assets discovered in serialized fields.
	std::vector<AssetKey> preload_assets{};

	std::optional<json> content{};
};

[[nodiscard]] SerializedScene LoadSceneFile(const path& file_path);
void SaveSceneFile(const path& file_path, const SerializedScene& scene);
[[nodiscard]] SerializedScene CaptureScene(const Scene& scene);

namespace impl {

/// @brief Finds catalog assets referenced by serialized scene parameters/content, then merges
/// explicit preload-only dependencies.
[[nodiscard]] std::vector<AssetKey> DiscoverSceneAssetDependencies(
	const Scene& scene,
	std::span<const AssetKey> explicit_dependencies = {}
);

[[nodiscard]] SceneFactory MakeSceneFactory(SerializedScene scene, bool runtime);

} // namespace impl

} // namespace ptgn
