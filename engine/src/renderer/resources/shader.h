#pragma once

#include <array>
#include <concepts>
#include <cstdint>
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

enum class ShaderStageMask : std::uint8_t {
	None = 0,
	Vertex = 1 << 0,
	Fragment = 1 << 1,
	VertexFragment = 3,
};
PTGN_REFLECT_ENUM(ShaderStageMask);

constexpr ShaderStageMask operator|(ShaderStageMask a, ShaderStageMask b) {
	return static_cast<ShaderStageMask>(
		static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
	);
}

constexpr ShaderStageMask operator&(ShaderStageMask a, ShaderStageMask b) {
	return static_cast<ShaderStageMask>(
		static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)
	);
}

constexpr bool HasShaderStage(ShaderStageMask stages, ShaderStageMask stage) {
	return static_cast<std::uint8_t>(stages & stage) != 0;
}

struct ShaderCode {
	constexpr ShaderCode() = default;

	constexpr explicit ShaderCode(std::string_view content, bool delete_after = true) :
		content{ content }, delete_after{ delete_after } {}

	std::string content;
	bool delete_after{ true };
};

struct ShaderPath {
	ShaderPath() = default;

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
	/// @brief If ShaderPathOrName, can either be a path or an already loaded engine shader name.
	std::variant<ShaderCode, ShaderPathOrName> vertex;
	/// @brief If ShaderPathOrName, can either be a path or an already loaded engine shader name.
	std::variant<ShaderCode, ShaderPathOrName> fragment;
};

[[nodiscard]] ShaderStageMask DetectShaderStages(std::string_view source);
[[nodiscard]] bool HasVertexAndFragmentShader(std::string_view source);

using UniformValue = std::variant<
	float, V2_float, V3_float, V4_float, std::vector<float>, int, V2_int, V3_int, V4_int,
	std::vector<int>, bool, Matrix4>;

struct UniformWrite {
	std::string name;
	UniformValue value;

	constexpr UniformWrite() = default;

	template <typename T>
		requires std::constructible_from<UniformValue, T&&>
	constexpr UniformWrite(std::string name, T&& value) :
		name{ std::move(name) }, value{ std::forward<T>(value) } {}

	constexpr bool operator==(const UniformWrite& o) const {
		if (value.index() != o.value.index() || name != o.name) {
			return false;
		}
		return std::visit(
			[&]<typename T>(const T& lhs) {
				const auto& rhs{ std::get<T>(o.value) };

				if constexpr (std::same_as<T, float> ||
							  std::same_as<T, std::vector<float>>) {
					return NearlyEqual(lhs, rhs);
				} else {
					return lhs == rhs;
				}
			},
			value
		);
	}

	PTGN_REFLECT(UniformWrite, name, value)
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

	friend std::ostream& operator<<(std::ostream& os, const Shader& shader) {
		os << "{ shader id: " << shader.operator impl::ShaderId() << " }";
		return os;
	}

	operator impl::ShaderId() const; // NOSONAR
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Shader> {
	std::size_t operator()(const ptgn::Shader& shader) const {
		return ptgn::Hash(shader.GetEntity());
	}
};
