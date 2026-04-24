#pragma once

#include <ecs/ecs.h>

#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn {

class Shader;

struct ShaderCode {
	ShaderCode() = default;

	explicit ShaderCode(const std::string& content, bool delete_after = true) :
		content{ content }, delete_after{ delete_after } {}

	std::string content;
	bool delete_after{ true };
};

struct ShaderPath {
	ShaderPath() = default;

	// Not explicit on purpose. Allows implicit conversion from path to ShaderPath, which is useful
	// for the common case of loading shaders from files.
	ShaderPath(const char* path, bool delete_after = true) : // NOSONAR
		path{ path }, delete_after{ delete_after } {}

	ShaderPath(const path& path, bool delete_after = true) : // NOSONAR
		path{ path }, delete_after{ delete_after } {}

	path path;
	bool delete_after{ true };
};

using ShaderName = std::string;

using ShaderPathOrName = std::string;

struct ShaderPair {
	/// @brief If ShaderPathOrName, can either be a path (if valid path) or a shader name for an
	/// already loaded shader in the asset manager.
	std::variant<ShaderCode, ShaderPathOrName> vertex;
	/// @brief If ShaderPathOrName, can either be a path (if valid path) or a shader name for an
	/// already loaded shader in the asset manager.
	std::variant<ShaderCode, ShaderPathOrName> fragment;
};

inline bool HasVertexAndFragmentShader(std::string_view source) {
	return source.contains("#type vertex") && source.contains("#type fragment");
}

namespace impl {

template <typename T>
concept UniformType = IsAnyOf<
	T, Matrix4, float, V2_float, V3_float, V4_float, std::vector<float>, int, V2_int, V3_int,
	V4_int, std::vector<int>, bool>;

class ShaderObject : public Resource<ShaderId> {
public:
	using Base = Resource<ShaderId>;
	using Base::Base;

	template <impl::UniformType T>
	void SetUniform(const char* uniform_name, const T& value);
};

} // namespace impl

class Shader : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	template <impl::UniformType T>
	void SetUniform(const char* uniform_name, const T& value) {
		GetEntity().Get<impl::ShaderObject>().SetUniform(uniform_name, value);
	}

	friend std::ostream& operator<<(std::ostream& os, const Shader& s) {
		os << "{ shader id: " << s.operator impl::ShaderId() << " }";
		return os;
	}

	// TODO: Consider moving this to private and not exposing any render functions that use ids.
	operator impl::ShaderId() const; // NOSONAR
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Shader> {
	std::size_t operator()(const ptgn::Shader& shader) const {
		return ptgn::Hash(shader.GetEntity());
	}
};