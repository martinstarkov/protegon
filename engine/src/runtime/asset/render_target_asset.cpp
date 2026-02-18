#include "runtime/asset/render_target_asset.h"

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_handle.h"

namespace ptgn {

void RenderTarget::Resize(V2_int new_size) {}

void RenderTarget::Bind() {}

void RenderTarget::Clear(Color color = color::Transparent) {}

V2_int RenderTarget::GetSize() const {}

TextureFormat RenderTarget::GetFormat() const {}

void RenderTarget::Destroy() {}

template class impl::RefCountedAsset<RenderTarget>;

} // namespace ptgn