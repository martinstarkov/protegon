#pragma once

#include <type_traits>

namespace ptgn::impl {

namespace gl {

class GLRenderer;

} // namespace gl

template <typename T>
class Resource {
public:
	// TODO: Move to concept requires, but last time I tried it messed with explicit instantiation.
	static_assert(std::is_copy_constructible_v<T>);

	Resource() = default;

	explicit Resource(gl::GLRenderer* renderer, T resource) noexcept;

	Resource(const Resource&)			 = delete;
	Resource& operator=(const Resource&) = delete;

	Resource(Resource&& other) noexcept;

	Resource& operator=(Resource&& other) noexcept;

	~Resource();

	operator T() const noexcept;

	explicit operator bool() const noexcept;

protected:
	void Reset() noexcept;

	gl::GLRenderer* renderer_{ nullptr };
	T resource_{};
};

} // namespace ptgn::impl