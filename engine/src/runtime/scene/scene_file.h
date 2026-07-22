#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"

namespace ptgn {

struct SerializedScene {
	std::string type;
	json parameters{ json::object() };
	std::optional<json> content;
};

[[nodiscard]] SerializedScene LoadSceneFile(const std::filesystem::path& path);
void SaveSceneFile(const std::filesystem::path& path, const SerializedScene& scene);

[[nodiscard]] SerializedScene CaptureScene(const Scene& scene);

namespace impl {

[[nodiscard]] SceneFactory MakeSceneFactory(SerializedScene scene, bool runtime);

} // namespace impl

} // namespace ptgn
