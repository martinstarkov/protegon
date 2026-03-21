#include "renderer/primitives/shader.h"

#include <ostream>
#include <vector>

#include "core/assert.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/entity_handle.h"
#include "ecs/ecs.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

template <typename T>
void ShaderObject::SetUniform(const char* uniform_name, const T& value) {
	PTGN_ASSERT(renderer_ != nullptr, "Renderer must be initialized before setting uniform");
	renderer_->SetUniform(resource_, uniform_name, value);
}

template void ShaderObject::SetUniform<float>(const char* uniform_name, const float& value);
template void ShaderObject::SetUniform<V2_float>(const char* uniform_name, const V2_float& value);
template void ShaderObject::SetUniform<V3_float>(const char* uniform_name, const V3_float& value);
template void ShaderObject::SetUniform<V4_float>(const char* uniform_name, const V4_float& value);
template void ShaderObject::SetUniform<std::vector<float>>(
	const char* uniform_name, const std::vector<float>& value
);
template void ShaderObject::SetUniform<int>(const char* uniform_name, const int& value);
template void ShaderObject::SetUniform<V2_int>(const char* uniform_name, const V2_int& value);
template void ShaderObject::SetUniform<V3_int>(const char* uniform_name, const V3_int& value);
template void ShaderObject::SetUniform<V4_int>(const char* uniform_name, const V4_int& value);
template void ShaderObject::SetUniform<std::vector<int>>(
	const char* uniform_name, const std::vector<int>& value
);
template void ShaderObject::SetUniform<bool>(const char* uniform_name, const bool& value);
template void ShaderObject::SetUniform<Matrix4>(const char* uniform_name, const Matrix4& value);

} // namespace impl

Shader::operator impl::ShaderId() const {
	return entity_.Get<impl::ShaderObject>();
}

std::ostream& operator<<(std::ostream& o, const Shader& s) {
	o << "{";
	o << "shader id: " << static_cast<impl::ShaderId>(s);
	o << "}";
	return o;
}

} // namespace ptgn
