#include "renderer/resources/shader.h"

#include "core/util/entity_handle.h"
#include "ecs/ecs.h"

namespace ptgn {

Shader::operator impl::ShaderId() const {
	return entity_.Get<impl::ShaderObject>();
}

} // namespace ptgn