#pragma once

#include <memory>

namespace ptgn {

template <typename T>
class Handle {
public:
	Handle() = default;
	Handle(const std::shared_ptr<T>& instance) : instance_{ instance } {}
	~Handle() = default;
	bool IsValid() const {
		return instance_ != nullptr;
	}
protected:
	std::shared_ptr<T> instance_;
private:

};
} // namespace ptgn
