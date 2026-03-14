#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"

namespace ptgn {

class Shader;
class AssetManager;

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
	ShaderPath(const char* path, bool delete_after = true) :
		path{ path }, delete_after{ delete_after } {}

	ShaderPath(const path& path, bool delete_after = true) :
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

struct ShaderTag {};

using ShaderId = Id<ShaderTag>;

class ShaderObject : public Resource<ShaderId> {
public:
	using Base = Resource<ShaderId>;
	using Base::Base;

	void SetUniform(const char* uniform_name, V2_float v);
	void SetUniform(const char* uniform_name, V3_float v);
	void SetUniform(const char* uniform_name, V4_float v);
	void SetUniform(const char* uniform_name, const Matrix4& matrix);
	void SetUniform(const char* uniform_name, const std::int32_t* data, std::int32_t count);
	void SetUniform(const char* uniform_name, const float* data, std::int32_t count);
	void SetUniform(const char* uniform_name, const Vector2<std::int32_t>& v);
	void SetUniform(const char* uniform_name, const Vector3<std::int32_t>& v);
	void SetUniform(const char* uniform_name, const Vector4<std::int32_t>& v);
	void SetUniform(const char* uniform_name, float v0);
	void SetUniform(const char* uniform_name, float v0, float v1);
	void SetUniform(const char* uniform_name, float v0, float v1, float v2);
	void SetUniform(const char* uniform_name, float v0, float v1, float v2, float v3);
	void SetUniform(const char* uniform_name, std::int32_t v0);
	void SetUniform(const char* uniform_name, std::int32_t v0, std::int32_t v1);
	void SetUniform(const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2);
	void SetUniform(
		const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
	);
	/// @brief Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const char* uniform_name, bool value);

private:
	friend class ptgn::Shader;

	void Bind();
};

} // namespace impl

class Shader : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	void SetUniform(const char* uniform_name, V2_float v);
	void SetUniform(const char* uniform_name, V3_float v);
	void SetUniform(const char* uniform_name, V4_float v);
	void SetUniform(const char* uniform_name, const Matrix4& matrix);
	void SetUniform(const char* uniform_name, const std::int32_t* data, std::int32_t count);
	void SetUniform(const char* uniform_name, const float* data, std::int32_t count);
	void SetUniform(const char* uniform_name, const Vector2<std::int32_t>& v);
	void SetUniform(const char* uniform_name, const Vector3<std::int32_t>& v);
	void SetUniform(const char* uniform_name, const Vector4<std::int32_t>& v);
	void SetUniform(const char* uniform_name, float v0);
	void SetUniform(const char* uniform_name, float v0, float v1);
	void SetUniform(const char* uniform_name, float v0, float v1, float v2);
	void SetUniform(const char* uniform_name, float v0, float v1, float v2, float v3);
	void SetUniform(const char* uniform_name, std::int32_t v0);
	void SetUniform(const char* uniform_name, std::int32_t v0, std::int32_t v1);
	void SetUniform(const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2);
	void SetUniform(
		const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
	);
	/// @brief Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const char* uniform_name, bool value);

	operator impl::ShaderId() const;

private:
	friend class AssetManager;

	void Bind();
};

} // namespace ptgn