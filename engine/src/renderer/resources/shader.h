#pragma once

#include <array>
#include <concepts>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "serialization/serialize.h"

namespace ptgn {

class Shader;

struct ShaderCode {
	constexpr ShaderCode() = default;

	constexpr explicit ShaderCode(std::string_view content, bool delete_after = true) :
		content{ content }, delete_after{ delete_after } {}

	std::string content;
	bool delete_after{ true };
};

struct ShaderPath {
	constexpr ShaderPath() = default;

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

[[nodiscard]] bool HasVertexAndFragmentShader(std::string_view source);

using UniformValue = std::variant<
	Matrix4, float, V2_float, V3_float, V4_float, std::vector<float>, int, V2_int, V3_int, V4_int,
	std::vector<int>, bool>;

struct UniformWrite {
	constexpr UniformWrite() = default;

	constexpr UniformWrite(std::string_view name, UniformValue value) :
		name{ std::string{ name } }, value{ std::move(value) } {}

	std::string name;
	UniformValue value;

	constexpr bool operator==(const UniformWrite& o) const {
		if (value.index() != o.value.index() || name != o.name) {
			return false;
		}
		return std::visit(
			[&]<typename T>(const T& lhs) {
				const auto& rhs{ std::get<T>(o.value) };

				if constexpr (std::same_as<T, float> || std::same_as<T, std::vector<float>>) {
					return NearlyEqual(lhs, rhs);
				} else {
					return lhs == rhs;
				}
			},
			value
		);
	}

	PTGN_SERIALIZE(UniformWrite, name, value)
};

[[nodiscard]] std::size_t Hash(const UniformValue& value);

namespace impl {

class ShaderObject : public Resource<ShaderId> {
public:
	using Base = Resource<ShaderId>;
	using Base::Base;

	void SetUniform(const char* uniform_name, const Matrix4& v);
	void SetUniform(const char* uniform_name, float v);
	void SetUniform(const char* uniform_name, V2_float v);
	void SetUniform(const char* uniform_name, V3_float v);
	void SetUniform(const char* uniform_name, V4_float v);
	void SetUniform(const char* uniform_name, std::span<const float> v);
	void SetUniform(const char* uniform_name, int v);
	void SetUniform(const char* uniform_name, V2_int v);
	void SetUniform(const char* uniform_name, V3_int v);
	void SetUniform(const char* uniform_name, V4_int v);
	void SetUniform(const char* uniform_name, std::span<const int> v);
	/// @brief Behaves identically to int overload.
	void SetUniform(const char* uniform_name, bool v);
};

} // namespace impl

class Shader : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	void SetUniform(const char* uniform_name, const Matrix4& v);
	void SetUniform(const char* uniform_name, float v);
	void SetUniform(const char* uniform_name, V2_float v);
	void SetUniform(const char* uniform_name, V3_float v);
	void SetUniform(const char* uniform_name, V4_float v);
	void SetUniform(const char* uniform_name, std::span<const float> v);
	void SetUniform(const char* uniform_name, int v);
	void SetUniform(const char* uniform_name, V2_int v);
	void SetUniform(const char* uniform_name, V3_int v);
	void SetUniform(const char* uniform_name, V4_int v);
	void SetUniform(const char* uniform_name, std::span<const int> v);
	/// @brief Behaves identically to int overload.
	void SetUniform(const char* uniform_name, bool v);

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