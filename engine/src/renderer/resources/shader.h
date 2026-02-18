#pragma once

#include <string>

#include "renderer/resources/id.h"

namespace ptgn {

struct ShaderCode {
	std::string content;
};

using ShaderName = std::string;

namespace impl {

struct ShaderTag {};

using Shader = Id<ShaderTag>;

} // namespace impl

} // namespace ptgn