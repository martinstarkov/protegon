#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "renderer/pipeline/shader_preprocessor.h"

namespace ptgn {

struct ShaderCompileResult {
	bool success{ false };
	std::string log{};
};

namespace impl {

/// @brief Validates one shader source. A two-stage source is compiled and linked. A single-stage
/// source is compiled without linking.
[[nodiscard]] ShaderCompileResult ValidateShaderSource(
	std::string_view source,
	std::size_t max_texture_slots
);

/// @brief Validates a vertex/fragment pair and links it as a temporary program.
[[nodiscard]] ShaderCompileResult ValidateShaderProgram(
	std::string_view vertex_source,
	std::string_view fragment_source,
	std::size_t max_texture_slots
);

} // namespace impl

} // namespace ptgn
