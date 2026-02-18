#pragma once

#include <cstdint>

#include "runtime/asset/asset_handle.h"

namespace ptgn {

class Shader : public impl::RefCountedAsset<Shader> {
public:
	using RefCountedAsset::RefCountedAsset;

private:
	friend class impl::RefCountedAsset<Shader>;

	void Destroy();
};

} // namespace ptgn