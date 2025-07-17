#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

#include "common/type_traits.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "serialization/serializable.h"
#include "utility/file.h"

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

	[[nodiscard]] T GetValue() const {
		return value_;
	}

	[[nodiscard]] T& GetValue() {
		return value_;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS(ArithmeticComponent, value_)

protected:
	T value_{};
};

struct HashComponent : public ArithmeticComponent<std::size_t> {
	using ArithmeticComponent::ArithmeticComponent;

	HashComponent(std::string_view key);

	HashComponent(const char* key);

	HashComponent(const std::string& key);
};

template <typename T, tt::enable<std::is_arithmetic_v<T>> = true>
struct Vector2Component {
	Vector2Component() = default;

	Vector2Component(const Vector2<T>& value) : value_{ value } {}

	operator Vector2<T>() const {
		return value_;
	}

	[[nodiscard]] Vector2<T> GetValue() const {
		return value_;
	}

	[[nodiscard]] Vector2<T>& GetValue() {
		return value_;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS(Vector2Component, value_)

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

	operator std::string() const {
		return value_;
	}

	[[nodiscard]] const std::string& GetValue() const {
		return value_;
	}

	[[nodiscard]] std::string& GetValue() {
		return value_;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS(StringComponent, value_)

protected:
	std::string value_;
};

} // namespace ptgn