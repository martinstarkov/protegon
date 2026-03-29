#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <variant>

#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "ecs/ecs.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"

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

class ShaderObject : public Resource<ShaderId> {
public:
	using Base = Resource<ShaderId>;
	using Base::Base;

	template <typename T>
	void SetUniform(const char* uniform_name, const T& value);
};

} // namespace impl

class Shader : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	template <typename T>
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

namespace std {

template <>
struct hash<ptgn::Shader> {
	std::size_t operator()(const ptgn::Shader& shader) const {
		return std::hash<ecs::Entity>()(shader.GetEntity());
	}
};

} // namespace std