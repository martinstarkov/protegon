#pragma once

namespace ptgn {

template <typename T>
class Event {
public:
	Event(T type) : type_{ type } {}
	T Type() const {
		return type_;
	}
protected:
	T type_;
};

} // namespace ptgn