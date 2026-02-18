#pragma once

#include <type_traits>
#include <utility>

namespace ptgn::impl {

template <typename T>
class Resource {
public:
	Resource() = default;

	explicit Resource(T resource) noexcept : resource_(resource) {}

	Resource(const Resource&)			 = delete;
	Resource& operator=(const Resource&) = delete;

	Resource(Resource&& other) noexcept : resource_{ std::exchange(other.resource_, T{}) } {}

	Resource& operator=(Resource&& other) noexcept {
		if (this != &other) {
			Reset();
			resource_ = std::exchange(other.resource_, T{});
		}
		return *this;
	}

	~Resource() {
		Reset();
	}

protected:
	T& Get() noexcept {
		return resource_;
	}

	const T& Get() const noexcept {
		return resource_;
	}

	bool Valid() const noexcept {
		return resource_ != T{};
	}

	void Reset() noexcept {
		if (resource_ != T{}) {
			static_cast<Derived*>(this)->Destroy(resource_);
			resource_ = T{};
		}
	}

private:
	T resource_{};
};

} // namespace ptgn::impl