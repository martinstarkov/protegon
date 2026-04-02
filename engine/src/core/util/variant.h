#pragma once

#include <optional>
#include <variant>

namespace ptgn {

template <typename... To, typename... From>
constexpr std::variant<To...> VariantCast(std::variant<From...>&& v) {
	return std::visit(
		[]<typename T>(T&& value) -> std::variant<To...> { return std::forward<T>(value); },
		std::move(v)
	);
}

template <typename... To, typename... From>
std::optional<std::variant<To...>> OptionalVariantCast(std::variant<From...>&& v) {
	return std::visit(
		[]<typename T>(T&& value) -> std::variant<To...> { return std::forward<T>(value); },
		std::move(v)
	);
}

template <typename... To, typename T>
constexpr std::optional<std::variant<To...>> OptionalVariantCast(std::optional<T>&& opt) {
	if (!opt.has_value()) {
		return std::nullopt;
	}
	return std::variant<To...>{ std::move(*opt) };
}

template <typename... To, typename... From>
constexpr std::optional<std::variant<To...>> OptionalVariantCast(
	std::optional<std::variant<From...>>&& opt
) {
	if (!opt.has_value()) {
		return std::nullopt;
	}
	return std::visit(
		[]<typename T>(T&& value) -> std::variant<To...> { return std::forward<T>(value); },
		std::move(*opt)
	);
}

} // namespace ptgn