#pragma once

#include <cstdint>

#include "runtime/asset/asset_handle.h"

namespace ptgn {

class Texture : public impl::RefCountedAsset<Texture> {
public:
	using RefCountedAsset::RefCountedAsset;

private:
	friend class impl::RefCountedAsset<Texture>;

	void Destroy();
};

} // namespace ptgn