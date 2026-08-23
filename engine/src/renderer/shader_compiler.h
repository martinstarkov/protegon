#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "renderer/resources/shader.h"

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

/// @return The requested raw #type block, including the source header, or an empty string.
[[nodiscard]] std::string ExtractShaderStageSource(
	std::string_view source,
	ShaderStageMask stage
);

/// @return A non-negative compatibility score when the vertex outputs satisfy the fragment inputs,
/// or -1 when the two stage interfaces are incompatible.
[[nodiscard]] int ShaderStageCompatibilityScore(
	std::string_view vertex_source,
	std::string_view fragment_source
);

} // namespace impl

} // namespace ptgn
