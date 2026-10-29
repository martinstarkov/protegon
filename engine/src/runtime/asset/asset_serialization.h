#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"
#include "serialization/serialize.h"

namespace ptgn {

inline constexpr std::string_view kShaderSourceToken{ "$source" };
inline constexpr std::string_view kBuiltinShaderPrefix{ "$builtin:" };

/// @brief Describes how a project shader program obtains each stage.
///
/// A value of "$source" uses the imported GLSL file. A value beginning with
/// "$builtin:" uses an engine-provided shader stage by name. A project-relative
/// path uses another imported GLSL file.
struct SerializedShaderProgram {
	std::optional<std::string> vertex{};
	std::optional<std::string> fragment{};

	PTGN_REFLECT(SerializedShaderProgram, vertex, fragment)
};

/// @brief Persistent path-backed asset entry stored in the project manifest.
/// Imported assets normally use project-relative paths. Assets registered through AssetManager::Load
/// may instead use runtime-root-relative or absolute paths when they live outside the project.
/// Loading state is deliberately not serialized; it is runtime residency state.
struct SerializedAsset {
	AssetKey key{};
	AssetKind kind{ AssetKind::Unknown };
	path source_path{};
	std::optional<SerializedShaderProgram> shader{};

	PTGN_REFLECT(SerializedAsset, key, kind, source_path, shader)
};

} // namespace ptgn
