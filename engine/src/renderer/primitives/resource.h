#pragma once

#include <type_traits>

namespace ptgn {

class Renderer;

namespace impl {

template <typename T>
concept ResourceType = std::is_copy_constructible_v<T>;

template <ResourceType T>
class Resource {
public:
	Resource() = default;

	explicit Resource(Renderer* renderer, T resource) noexcept;

	Resource(const Resource&)			 = delete;
	Resource& operator=(const Resource&) = delete;

	Resource(Resource&& other) noexcept;

	Resource& operator=(Resource&& other) noexcept;

	~Resource() noexcept;

	operator T() const; // NOSONAR

	explicit operator bool() const;

protected:
	friend class ptgn::Renderer;

	void Reset() noexcept;

	Renderer* renderer_{ nullptr };
	T resource_{};
};

} // namespace impl

} // namespace ptgn