#pragma once

#include <memory>

#include "common/assert.h"

namespace ptgn {

// Since a handle is a wrapper around a smart pointer, copying a handle does not result in a new
// instance of the object.
template <typename T>
class Handle {
public:
	Handle() = default;

	Handle(const std::shared_ptr<T>& copy) : instance_{ copy } {}

	bool IsValid() const {
		return instance_ != nullptr;
	}

	bool operator==(const Handle&) const = default;

protected:
	void Set(const std::shared_ptr<T>& other) {
		instance_ = other;
	}

	const T& Get() const {
		PTGN_ASSERT(IsValid(), "Uninitialized instance");
		return *instance_;
	}

	T& Get() {
		PTGN_ASSERT(IsValid(), "Uninitialized instance");
		return *instance_;
	}

	template <typename... TArgs>
	void Create(TArgs&&... args) {
		if (!IsValid()) {
			instance_ = std::make_shared<T>(std::forward<TArgs>(args)...);
		}
	}

	void Create(std::shared_ptr<T> instance) {
		PTGN_ASSERT(!IsValid(), "Cannot recreate instance");
		instance_ = instance;
	}

	void Destroy() {
		instance_ = nullptr;
	}

	[[nodiscard]] const std::shared_ptr<T>& GetPtr() const {
		return instance_;
	}

private:
	std::shared_ptr<T> instance_;
};

} // namespace ptgn
