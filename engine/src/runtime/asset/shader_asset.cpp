#include "runtime/asset/shader_asset.h"

#include "ecs/ecs.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/asset_handle.h"

namespace ptgn {

Shader::operator impl::ShaderId() const {
	return entity_.Get<impl::ShaderObject>();
}

} // namespace ptgn