#pragma once

#include <algorithm>
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/serialize.h"

namespace ptgn {

inline constexpr float kMinLineWidth{ 1.0f };

struct Solid {
	constexpr Solid() = default;

	PTGN_SERIALIZE_EMPTY(Solid)
};

struct Hollow {
	constexpr Hollow() = default;

	constexpr Hollow(float line_width) : line_width{ line_width } { // NOSONAR
		PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);
	}

	float line_width{ kMinLineWidth }; // must be positive and >= kMinLineWidth

	PTGN_SERIALIZE_VALUE(Hollow, line_width)
};

class FillStyle {
private:
	using Variant = std::variant<Solid, Hollow>;

public:
	constexpr FillStyle() = default;

	constexpr FillStyle(float line_width) : style_{ Hollow{ line_width } } { // NOSONAR
	}

	constexpr FillStyle(Solid) : style_{ Solid{} } {} // NOSONAR

	[[nodiscard]] constexpr bool IsHollow() const {
		return std::holds_alternative<Hollow>(style_);
	}

	[[nodiscard]] constexpr bool IsSolid() const {
		return std::holds_alternative<Solid>(style_);
	}

	/// @brief GetLineWidth() returns the line width if this FillStyle is hollow,
	/// and std::nullopt otherwise.
	[[nodiscard]] constexpr std::optional<float> GetLineWidth() const {
		if (auto hollow{ std::get_if<Hollow>(&style_) }) {
			return hollow->line_width;
		}
		return std::nullopt;
	}

	template <typename F>
		requires VariantVisitor<F, Variant>
	constexpr decltype(auto) Visit(F&& fn) const {
		return std::visit(std::forward<F>(fn), style_);
	}

	template <Invocable FSolid, Invocable<float> FHollow>
	constexpr auto Apply(FSolid solid_fn, FHollow hollow_fn) {
		using R1 = std::invoke_result_t<FSolid>;
		using R2 = std::invoke_result_t<FHollow, float>;

		if constexpr (std::same_as<R1, R2>) {
			return Visit([&]<typename T>(const T& s) -> R1 {
				if constexpr (std::is_same_v<T, Solid>) {
					return solid_fn();
				} else if constexpr (std::is_same_v<T, Hollow>) {
					PTGN_ASSERT(s.line_width >= kMinLineWidth);
					return hollow_fn(s.line_width);
				} else {
					static_assert(false, "Incomplete visitor");
				}
			});
		} else {
			using R = std::variant<R1, R2>;

			return Visit([&]<typename T>(const T& s) -> R {
				if constexpr (std::is_same_v<T, Solid>) {
					return R{ solid_fn() };
				} else if constexpr (std::is_same_v<T, Hollow>) {
					PTGN_ASSERT(s.line_width >= kMinLineWidth);
					return R{ hollow_fn(s.line_width) };
				} else {
					static_assert(false, "Incomplete visitor");
				}
			});
		}
	}

	/// @brief Converts a fill style to a SDF line thickness for shaders to draw hollow and solid
	/// shapes.
	[[nodiscard]] constexpr float NormalizedToSDFThickness(float fade, V2_float radii) const {
		return Visit([fade, radii]<typename T>(const T& s) {
			if constexpr (std::is_same_v<T, Solid>) {
				// Internally line width for a filled SDF is 1.0f.
				return 1.0f;
			} else if constexpr (std::is_same_v<T, Hollow>) {
				PTGN_ASSERT(s.line_width >= kMinLineWidth, "Invalid line width for circle");

				// Internally line width for a completely hollow ellipse is 0.0f.
				return fade + s.line_width / std::min(radii.x, radii.y);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		});
	}

	PTGN_SERIALIZE_VALUE(FillStyle, style_)

private:
	Variant style_{};
};

} // namespace ptgn