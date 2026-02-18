#include "runtime/asset/shader_asset.h"

#include "runtime/asset/asset_handle.h"

namespace ptgn {

void Shader::Destroy() {}

template class impl::RefCountedAsset<Shader>;

} // namespace ptgn