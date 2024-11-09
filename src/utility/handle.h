#pragma once

#include <memory>

#include "utility/debug.h"

namespace ptgn {

template <typename T>
class Handle {
public:
	Handle() = default;

	Handle(const std::shared_ptr<T>& copy) : instance_{ copy } {}

	virtual ~Handle() = default;

	bool IsValid() const {
		return instance_ != nullptr;
	}

	friend bool operator==(const Handle& a, const Handle& b) {
		return a.instance_ == b.instance_;
	}

	friend bool operator!=(const Handle& a, const Handle& b) {
		return !(a == b);
	}

	/*std::shared_ptr<T> Copy() const {
		PTGN_ASSERT(IsValid(), "Cannot copy invalid handle");
		return std::make_shared<T>(*instance_);
	}*/

protected:
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

private:
	std::shared_ptr<T> instance_;
};

} // namespace ptgn
