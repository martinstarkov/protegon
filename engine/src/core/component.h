#pragma once

#include <string>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct ColorComponent : public Color {
	using Color::Color;
	using Color::operator=;

	ColorComponent(Color c) : Color{ c } {}
};

template <Arithmetic T>
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

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(ArithmeticComponent, value_)

protected:
	T value_{};
};

template <Arithmetic T>
struct Vector2Component {
	Vector2Component() = default;

	Vector2Component(Vector2<T> value) : value_{ value } {}

	operator Vector2<T>() const {
		return value_;
	}

	[[nodiscard]] Vector2<T> GetValue() const {
		return value_;
	}

	[[nodiscard]] Vector2<T>& GetValue() {
		return value_;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(Vector2Component, value_)

protected:
	Vector2<T> value_{ 0 };
};

struct StringComponent {
	StringComponent() = default;

	StringComponent(const path& p) = delete;

	StringComponent(const std::string& value) : value_{ value } {}

	StringComponent(std::string_view value) : value_{ value } {}

	StringComponent(const char* value) : value_{ value } {}

	bool operator==(const StringComponent&) const = default;

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

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(StringComponent, value_)

protected:
	std::string value_;
};

struct HashComponent {
	HashComponent() = default;

	HashComponent(std::string_view key) : value_{ Hash(key) } {}

	HashComponent(const char* key) : value_{ Hash(key) } {}

	HashComponent(const std::string& key) : value_{ Hash(key) } {}

	HashComponent(std::size_t value) : value_{ value } {}

	operator std::size_t() const {
		return value_;
	}

	std::size_t GetHash() const {
		return value_;
	}

	std::size_t& GetHash() {
		return value_;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(HashComponent, value_)
protected:
	std::size_t value_{ 0 };
};

} // namespace ptgn