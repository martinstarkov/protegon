#include "renderer/resources/shader.h"

#include <ecs/ecs.h>

#include <span>
#include <string_view>
#include <variant>

#include "core/assert.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn {

namespace impl {

template <typename T>
concept ShaderUniformSettable = requires(ShaderObject& object, const char* name, T&& value) {
	object.SetUniform(name, std::forward<T>(value));
};

template <ShaderUniformSettable T>
void SetUniformImpl(Renderer* renderer, ShaderId id, const char* uniform_name, T&& v) {
	PTGN_ASSERT(renderer, "Renderer must be initialized before setting uniform");
	renderer->SetUniform(id, uniform_name, std::forward<T>(v));
}

void ShaderObject::SetUniform(const char* uniform_name, const Matrix4& v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, float v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V2_float v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V3_float v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V4_float v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, std::span<const float> v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, bool v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, int v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V2_int v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V3_int v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V4_int v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, std::span<const int> v) {
	SetUniformImpl(renderer_, resource_, uniform_name, v);
}

} // namespace impl

bool HasVertexAndFragmentShader(std::string_view source) {
	return source.contains("#type vertex") && source.contains("#type fragment");
}

std::size_t Hash(const UniformValue& value) {
	return std::visit([](const auto& v) { return Hash(v); }, value);
}

void Shader::SetUniform(const char* uniform_name, const Matrix4& v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, float v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V2_float v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V3_float v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V4_float v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, std::span<const float> v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, int v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V2_int v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V3_int v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V4_int v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, std::span<const int> v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, bool v) {
	GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

Shader::operator impl::ShaderId() const {
	return GetEntity().Get<impl::ShaderObject>();
}

} // namespace ptgn
