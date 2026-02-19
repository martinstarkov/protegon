#pragma once

#include "renderer/resources/shader.h"
#include "runtime/asset/asset_handle.h"

namespace ptgn {

class Shader : public impl::Asset {
public:
	using Base = impl::Asset;
	using Base::Base;

	operator impl::ShaderId() const;
};

} // namespace ptgn