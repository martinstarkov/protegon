#pragma once

#include <string>

#include "core/util/entity_handle.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn {

class AssetManager;

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

class Shader : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	operator impl::ShaderId() const;

private:
	friend class AssetManager;
};

} // namespace ptgn