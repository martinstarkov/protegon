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

using ShaderId = Id<ShaderTag>;

class ShaderObject : public Resource<ShaderId> {
public:
	using Base = Resource<ShaderId>;
	using Base::Base;
};

} // namespace impl

} // namespace ptgn