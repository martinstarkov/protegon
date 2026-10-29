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

	explicit Resource(Renderer* renderer_ptr, T render_resource) noexcept;

	Resource(const Resource&)			 = delete;
	Resource& operator=(const Resource&) = delete;

	Resource(Resource&& other) noexcept;

	Resource& operator=(Resource&& other) noexcept;

	~Resource() noexcept;

	operator T() const; // NOSONAR

	explicit operator bool() const;

	bool operator==(const Resource&) const = default;

	Renderer* renderer{ nullptr };
	T resource{};

private:
	void Reset() noexcept;
};

} // namespace impl

} // namespace ptgn