#pragma once

#include <span>
#include <string>

#include "core/util/file.h"
#include "serialization/json/fwd.h"

namespace ptgn::impl {

/// @brief Immutable source for a shader embedded with the engine binary.
///
/// The embedded shader library lives in the asset subsystem so AssetManager and the renderer
/// backend consume the same source catalog instead of independently reading CMRC resources.
struct EngineShaderFile {
	path filename;
	std::string source;
};

/// @return Engine GLSL source files embedded in the application.
[[nodiscard]] std::span<const EngineShaderFile> GetEngineShaderFiles();

/// @return Engine shader-program manifest embedded in the application.
[[nodiscard]] const json& GetEngineShaderManifest();

} // namespace ptgn::impl
