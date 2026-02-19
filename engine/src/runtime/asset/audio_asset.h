#pragma once

#include "runtime/asset/asset_handle.h"

namespace ptgn {

class Audio : public impl::Asset {
public:
	using Base = impl::Asset;
	using Base::Base;
};

} // namespace ptgn