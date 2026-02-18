#pragma once

#include <cstdint>

#include "runtime/asset/asset_handle.h"

namespace ptgn {

class RenderTarget : public impl::RefCountedAsset<RenderTarget> {
public:
	using RefCountedAsset::RefCountedAsset;

private:
	friend class impl::RefCountedAsset<RenderTarget>;

	void Destroy();
};

} // namespace ptgn