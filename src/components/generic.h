#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "utility/file.h"
#include "utility/function.h"
#include "utility/type_traits.h"

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

	[[nodiscard]] T& GetValue() {
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

protected:
	Vector2<T> value_{ 0 };
};

struct StringComponent {
	StringComponent() = default;

	StringComponent(const path& p) = delete;

	StringComponent(const std::string& value) : value_{ value } {}

	StringComponent(std::string_view value) : value_{ value } {}

	StringComponent(const char* value) : value_{ value } {}

	friend bool operator==(const StringComponent& a, const StringComponent& b) {
		return a.value_ == b.value_;
	}

	friend bool operator!=(const StringComponent& a, const StringComponent& b) {
		return !(a == b);
	}

	operator std::string_view() const {
		return value_;
	}

protected:
	std::string value_;
};

// TODO: Get rid of this in favor of scripts.
template <typename... TArgs>
struct CallbackComponent {
	CallbackComponent() = default;

	CallbackComponent(const std::function<void(TArgs...)>& callback) : callback_{ callback } {}

	void operator()(TArgs... args) const {
		Invoke(callback_, args...);
	}

	bool operator==(std::nullptr_t) const {
		return callback_ == nullptr;
	}

	bool operator!=(std::nullptr_t) const {
		return callback_ != nullptr;
	}

	operator std::function<void(TArgs...)>() const {
		return callback_;
	}

protected:
	std::function<void(TArgs...)> callback_{};
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