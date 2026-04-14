#pragma once

#include <concepts>
#include <ranges>
#include <vector>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Polygon {
public:
	constexpr Polygon() = default;

	template <typename Container> // NOSONAR
		requires std::ranges::input_range<Container> &&
				 std::convertible_to<std::ranges::range_value_t<Container>, V2_float>
	constexpr Polygon(const Container& vertices) { // NOSONAR
		vertices_.assign(vertices.begin(), vertices.end());
	}

	template <typename Container> // NOSONAR
		requires std::ranges::input_range<Container> &&
				 std::convertible_to<std::ranges::range_value_t<Container>, V2_float>
	void SetVertices(const Container& vertices) {
		vertices_.assign(vertices.begin(), vertices.end());
	}

	const std::vector<V2_float>& GetVertices() const;
	std::vector<V2_float>& GetVertices();

	constexpr V2_float* Data() noexcept {
		return vertices_.data();
	}

	constexpr const V2_float* Data() const noexcept {
		return vertices_.data();
	}

	constexpr std::size_t GetVertexCount() const {
		return vertices_.size();
	}

	constexpr auto begin() noexcept {
		return vertices_.begin();
	}

	constexpr auto end() noexcept {
		return vertices_.end();
	}

	constexpr auto begin() const noexcept {
		return vertices_.begin();
	}

	constexpr auto end() const noexcept {
		return vertices_.end();
	}

	std::vector<V2_float> GetWorldVertices(Transform transform) const;

	std::vector<V2_float> GetLocalVertices() const;

	/// @return Centroid of the polygon.
	V2_float GetCenter() const;

	bool operator==(const Polygon&) const = default;

	PTGN_REFLECT(Polygon, vertices_)

private:
	std::vector<V2_float> vertices_;
};

} // namespace ptgn