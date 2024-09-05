#pragma once

#include <memory>

namespace ptgn {

template <typename T>
class Handle {
public:
	Handle() = default;

	Handle(const std::shared_ptr<T>& instance) : instance_{ instance } {}

	virtual ~Handle() = default;

	bool IsValid() const {
		return instance_ != nullptr;
	}

	std::shared_ptr<T>& GetInstance() {
		return instance_;
	}

	const std::shared_ptr<T>& GetInstance() const {
		return instance_;
	}

protected:
	std::shared_ptr<T> instance_;

private:
};
} // namespace ptgn
