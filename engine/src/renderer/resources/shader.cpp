#include "renderer/resources/shader.h"

#include <ecs/ecs.h>

#include <span>
#include <string_view>
#include <variant>

#include "core/assert.h"
#include "core/log.h"
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

#define PTGN_SET_UNIFORM                                                          \
	PTGN_ASSERT(renderer, "Renderer must be initialized before setting uniform"); \
	renderer->SetUniform(resource, uniform_name, v);

void ShaderObject::SetUniform(const char* uniform_name, const Matrix4& v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, float v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, V2_float v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, V3_float v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, V4_float v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, std::span<const float> v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, bool v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, int v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, V2_int v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, V3_int v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, V4_int v) {
	PTGN_SET_UNIFORM
}

void ShaderObject::SetUniform(const char* uniform_name, std::span<const int> v) {
	PTGN_SET_UNIFORM
}

#undef PTGN_SET_UNIFORM

} // namespace impl

ShaderStageMask DetectShaderStages(std::string_view source) {
	ShaderStageMask stages{ ShaderStageMask::None };

	if (source.contains("#type vertex")) {
		stages = stages | ShaderStageMask::Vertex;
	}
	if (source.contains("#type fragment")) {
		stages = stages | ShaderStageMask::Fragment;
	}

	return stages;
}

bool HasVertexAndFragmentShader(std::string_view source) {
	return DetectShaderStages(source) == ShaderStageMask::VertexFragment;
}

std::size_t Hash(const UniformValue& value) {
	return std::visit([](const auto& v) { return Hash(v); }, value);
}

namespace {

impl::ShaderObject* TryShaderObject(Shader& shader) {
	if (!shader) {
		PTGN_WARN("Attempted to use an unavailable shader; draw/update skipped");
		return nullptr;
	}
	auto object{ shader.GetEntity().TryGet<impl::ShaderObject>() };
	if (!object) {
		PTGN_WARN("Attempted to use a shader that has no compiled program; draw/update skipped");
		return nullptr;
	}
	return object;
}

} // namespace

#define PTGN_SHADER_SAFE_UNIFORM(TYPE) \
void Shader::SetUniform(const char* uniform_name, TYPE v) { \
	if (auto* object{ TryShaderObject(*this) }) { object->SetUniform(uniform_name, v); } \
}

void Shader::SetUniform(const char* uniform_name, const Matrix4& v) {
	if (auto* object{ TryShaderObject(*this) }) { object->SetUniform(uniform_name, v); }
}
PTGN_SHADER_SAFE_UNIFORM(float)
PTGN_SHADER_SAFE_UNIFORM(V2_float)
PTGN_SHADER_SAFE_UNIFORM(V3_float)
PTGN_SHADER_SAFE_UNIFORM(V4_float)
void Shader::SetUniform(const char* uniform_name, std::span<const float> v) {
	if (auto* object{ TryShaderObject(*this) }) { object->SetUniform(uniform_name, v); }
}
PTGN_SHADER_SAFE_UNIFORM(int)
PTGN_SHADER_SAFE_UNIFORM(V2_int)
PTGN_SHADER_SAFE_UNIFORM(V3_int)
PTGN_SHADER_SAFE_UNIFORM(V4_int)
void Shader::SetUniform(const char* uniform_name, std::span<const int> v) {
	if (auto* object{ TryShaderObject(*this) }) { object->SetUniform(uniform_name, v); }
}
PTGN_SHADER_SAFE_UNIFORM(bool)
#undef PTGN_SHADER_SAFE_UNIFORM

Shader::operator impl::ShaderId() const {
	if (!*this) {
		PTGN_WARN("Attempted to use an unavailable shader; rendering skipped");
		return {};
	}
	if (auto object{ GetEntity().TryGet<impl::ShaderObject>() }) {
		return *object;
	}
	PTGN_WARN("Attempted to use a shader with compile errors; rendering skipped");
	return {};
}

} // namespace ptgn
