#include "renderer/primitives/shader.h"

#include <cstdint>
#include <memory>

#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/entity_handle.h"
#include "ecs/ecs.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/primitives/resource.h"

namespace ptgn {

namespace impl {

void ShaderObject::Bind() {
	auto _ = renderer_->gl->Bind(resource_);
}

void ShaderObject::SetUniform(const char* uniform_name, V2_float v) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V3_float v) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, V4_float v) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, const Matrix4& matrix) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, matrix);
}

void ShaderObject::SetUniform(
	const char* uniform_name, const std::int32_t* data, std::int32_t count
) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, data, count);
}

void ShaderObject::SetUniform(const char* uniform_name, const float* data, std::int32_t count) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, data, count);
}

void ShaderObject::SetUniform(const char* uniform_name, const Vector2<std::int32_t>& v) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, const Vector3<std::int32_t>& v) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, const Vector4<std::int32_t>& v) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v);
}

void ShaderObject::SetUniform(const char* uniform_name, float v0) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0);
}

void ShaderObject::SetUniform(const char* uniform_name, float v0, float v1) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0, v1);
}

void ShaderObject::SetUniform(const char* uniform_name, float v0, float v1, float v2) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0, v1, v2);
}

void ShaderObject::SetUniform(const char* uniform_name, float v0, float v1, float v2, float v3) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0, v1, v2, v3);
}

void ShaderObject::SetUniform(const char* uniform_name, std::int32_t v0) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0);
}

void ShaderObject::SetUniform(const char* uniform_name, std::int32_t v0, std::int32_t v1) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0, v1);
}

void ShaderObject::SetUniform(
	const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0, v1, v2);
}

void ShaderObject::SetUniform(
	const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, v0, v1, v2, v3);
}

void ShaderObject::SetUniform(const char* uniform_name, bool value) {
	renderer_->gl->shaders.SetUniform(resource_, uniform_name, value);
}

} // namespace impl

Shader::operator impl::ShaderId() const {
	return entity_.Get<impl::ShaderObject>();
}

void Shader::Bind() {
	entity_.Get<impl::ShaderObject>().Bind();
}

void Shader::SetUniform(const char* uniform_name, V2_float v) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V3_float v) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, V4_float v) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, const Matrix4& matrix) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, matrix);
}

void Shader::SetUniform(const char* uniform_name, const std::int32_t* data, std::int32_t count) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, data, count);
}

void Shader::SetUniform(const char* uniform_name, const float* data, std::int32_t count) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, data, count);
}

void Shader::SetUniform(const char* uniform_name, const Vector2<std::int32_t>& v) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, const Vector3<std::int32_t>& v) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, const Vector4<std::int32_t>& v) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v);
}

void Shader::SetUniform(const char* uniform_name, float v0) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0);
}

void Shader::SetUniform(const char* uniform_name, float v0, float v1) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0, v1);
}

void Shader::SetUniform(const char* uniform_name, float v0, float v1, float v2) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0, v1, v2);
}

void Shader::SetUniform(const char* uniform_name, float v0, float v1, float v2, float v3) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0, v1, v2, v3);
}

void Shader::SetUniform(const char* uniform_name, std::int32_t v0) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0);
}

void Shader::SetUniform(const char* uniform_name, std::int32_t v0, std::int32_t v1) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0, v1);
}

void Shader::SetUniform(
	const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0, v1, v2);
}

void Shader::SetUniform(
	const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, v0, v1, v2, v3);
}

void Shader::SetUniform(const char* uniform_name, bool value) {
	entity_.Get<impl::ShaderObject>().SetUniform(uniform_name, value);
}

} // namespace ptgn
