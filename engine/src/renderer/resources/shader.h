#pragma once

#include <string>

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn {

struct ShaderCode {
	std::string content;
};

using ShaderName = std::string;

namespace impl {

struct ShaderTag {};

using Shader = Id<ShaderTag>;

class ShaderObject : public Resource<Shader> {
public:
	using Base = Resource<Shader>;
	using Base::Base;
};

} // namespace impl

} // namespace ptgn