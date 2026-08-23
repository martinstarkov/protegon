#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "renderer/resources/shader.h"

namespace ptgn::impl {

inline constexpr const char* kTexturesUniform{ "u_Textures" };

struct PreparedShaderStage {
	ShaderStageMask stage{ ShaderStageMask::None };
	std::string source{};
};

using ShaderPreprocessResult = std::expected<std::vector<PreparedShaderStage>, std::string>;

/// @brief Parses Protegon shader source, applies shader options, injects the platform preamble,
/// and substitutes engine shader tokens.
[[nodiscard]] ShaderPreprocessResult PrepareShaderSource(
	std::string_view source,
	std::size_t max_texture_slots
);

/// @return The requested raw #type block, including the shared source header, or an empty string.
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

[[nodiscard]] std::string_view ShaderStageName(ShaderStageMask stage);

} // namespace ptgn::impl
