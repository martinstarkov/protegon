#pragma once

#include <functional>
#include <string_view>
#include <type_traits>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "utility/type_traits.h"
#include "utility/utility.h"

namespace ptgn {

struct ColorComponent : public Color {
	using Color::Color;
	using Color::operator=;

	ColorComponent(const Color& c) : Color{ c } {}
};

template <typename T, tt::enable<std::is_arithmetic_v<T>> = true>
struct ArithmeticComponent {
	ArithmeticComponent() = default;

	ArithmeticComponent(T value) : value_{ value } {}

	operator T() const {
		return value_;
	}

protected:
	T value_{};
};

template <typename T, tt::enable<std::is_arithmetic_v<T>> = true>
struct Vector2Component {
	Vector2Component() = default;

	Vector2Component(const Vector2<T>& value) : value_{ value } {}

	operator Vector2<T>() const {
		return value_;
	}

private:
	Vector2<T> value_{ 0 };
};

struct StringViewComponent {
	StringViewComponent() = default;

	StringViewComponent(std::string_view value) : value_{ value } {}

	bool operator==(const StringViewComponent& other) const {
		return value_ == other.value_;
	}

	bool operator!=(const StringViewComponent& other) const {
		return !(*this == other);
	}

	operator std::string_view() const {
		return value_;
	}

private:
	std::string_view value_{};
};

template <typename TReturn, typename... TArgs>
struct CallbackComponent {
	CallbackComponent() = default;

	CallbackComponent(const std::function<TReturn(TArgs...)>& callback) : callback_{ callback } {}

	TReturn operator()(TArgs&&... args) const {
		return Invoke(callback_, std::forward<TArgs>(args)...);
	}

	bool operator==(std::nullptr_t) const {
		return callback_ == nullptr;
	}

	bool operator!=(std::nullptr_t) const {
		return callback_ != nullptr;
	}

	operator std::function<TReturn(TArgs...)>() const {
		return callback_;
	}

protected:
	std::function<TReturn(TArgs...)> callback_{};
};

struct OriginComponent {
	OriginComponent() = default;

	OriginComponent(Origin origin) : origin_{ origin } {}

	operator Origin() const {
		return origin_;
	}

protected:
	Origin origin_{ Origin::Center };
};

} // namespace ptgn