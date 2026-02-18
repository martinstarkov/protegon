#pragma once

#include <type_traits>

namespace ptgn::impl {

namespace gl {

class Renderer;

} // namespace gl

template <typename T>
class Resource {
public:
	// TODO: Move to requires.
	static_assert(std::is_copy_constructible_v<T>);

	Resource() = default;

	explicit Resource(gl::Renderer* renderer, T resource) noexcept;

	Resource(const Resource&)			 = delete;
	Resource& operator=(const Resource&) = delete;

	Resource(Resource&& other) noexcept;

	Resource& operator=(Resource&& other) noexcept;

	~Resource();

	operator T() const noexcept;

protected:
	bool IsValid() const noexcept;

	void Reset() noexcept;

	gl::Renderer* renderer_{ nullptr };
	T resource_{};
};

} // namespace ptgn::impl