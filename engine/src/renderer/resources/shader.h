#pragma once

#include <string>

namespace ptgn {

// Wrapper for distinguishing between Shader from path construction and Shader
// from source construction.
struct ShaderCode {
	std::string source;
};

namespace impl {

// TODO: Move shader parsing code here.

} // namespace impl

} // namespace ptgn