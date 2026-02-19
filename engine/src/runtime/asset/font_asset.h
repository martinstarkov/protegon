#pragma once

#include "runtime/asset/asset_handle.h"

namespace ptgn {

namespace impl {

struct FontSize {
	float point_size{ 0.0f };
};

} // namespace impl

class Font : public impl::Asset {
public:
	using Base = impl::Asset;
	using Base::Base;
};

} // namespace ptgn