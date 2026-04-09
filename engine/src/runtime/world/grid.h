#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"

// TODO: Add serialization.

namespace ptgn {

template <typename T>
	requires std::is_default_constructible_v<T> && std::is_move_constructible_v<T>
class Grid {
public:
	Grid() = default;

	explicit Grid(V2_int grid_dimensions, const std::vector<T>& grid_cells) :
		size{ grid_dimensions },
		length{ grid_dimensions.x * grid_dimensions.y },
		cells{ grid_cells } {
		PTGN_ASSERT(static_cast<std::size_t>(length) == cells.size(), "Failed to construct grid");
	}

	explicit Grid(V2_int grid_dimensions) :
		size{ grid_dimensions },
		length{ grid_dimensions.x * grid_dimensions.y },
		cells(static_cast<std::size_t>(length)) {
		PTGN_ASSERT(static_cast<std::size_t>(length) == cells.size(), "Failed to construct grid");
	}

	template <InvocableR<void, V2_int> F>
	void ForEachCoordinate(F&& func) const {
		for (int i{ 0 }; i < size.x; i++) {
			for (int j{ 0 }; j < size.y; j++) {
				func(V2_int{ i, j });
			}
		}
	}

	template <InvocableR<void, V2_int, const T&> F>
	void ForEach(F&& func) const {
		for (int i{ 0 }; i < size.x; i++) {
			for (int j{ 0 }; j < size.y; j++) {
				V2_int coordinate{ i, j };
				func(coordinate, Get(coordinate));
			}
		}
	}

	template <InvocableR<void, V2_int, T&> F>
	void ForEach(F&& func) {
		for (int i{ 0 }; i < size.x; i++) {
			for (int j{ 0 }; j < size.y; j++) {
				V2_int coordinate{ i, j };
				func(coordinate, Get(coordinate));
			}
		}
	}

	template <InvocableR<void, int> F>
	void ForEachIndex(F&& func) const {
		for (int i{ 0 }; i < length; i++) {
			func(i);
		}
	}

	template <InvocableR<void, T&> F>
	void ForEachElement(F&& func) {
		for (auto& cell : cells) {
			func(cell);
		}
	}

	template <InvocableR<void, const T&> F>
	void ForEachElement(F&& func) const {
		for (auto& cell : cells) {
			func(cell);
		}
	}

	[[nodiscard]] bool Has(V2_int coordinate) const {
		if (coordinate.x < 0 || coordinate.y < 0) {
			return false;
		}
		if (coordinate.x >= size.x || coordinate.y >= size.y) {
			return false;
		}
		return true;
	}

	[[nodiscard]] bool Has(int index) const {
		return index >= 0 && index < length;
	}

	const T& Get(V2_int coordinate) const {
		auto c{ OneDimensionalize(coordinate) };
		PTGN_ASSERT(c.has_value(), "Coordinate out of range");
		return Get(*c);
	}

	[[nodiscard]] T Pop(V2_int coordinate) {
		auto c{ OneDimensionalize(coordinate) };
		PTGN_ASSERT(c.has_value(), "Coordinate out of range");
		return Pop(*c);
	}

	T& Get(V2_int coordinate) {
		auto c{ OneDimensionalize(coordinate) };
		PTGN_ASSERT(c.has_value(), "Coordinate out of range");
		return Get(*c);
	}

	[[nodiscard]] T Pop(int index) {
		PTGN_ASSERT(Has(index), "Cannot pop grid element which is outside the grid");
		T popped{ std::move(cells[static_cast<std::size_t>(index)]) };
		cells[static_cast<std::size_t>(index)] = T{};
		return popped;
	}

	const T& Get(int index) const {
		PTGN_ASSERT(Has(index), "Cannot get grid element which is outside the grid");
		return cells[static_cast<std::size_t>(index)];
	}

	T& Get(int index) {
		return const_cast<T&>(std::as_const(*this).Get(index));
	}

	T& Set(V2_int coordinate, T&& object) {
		auto c{ OneDimensionalize(coordinate) };
		PTGN_ASSERT(c.has_value(), "Coordinate out of range");
		return Set(*c, std::move(object));
	}

	T& Set(int index, T&& object) {
		PTGN_ASSERT(Has(index), "Cannot set grid element which is outside the grid");
		auto& value = cells[static_cast<std::size_t>(index)];
		value		= std::move(object);
		return value;
	}

	void Clear() {
		cells.clear();
	}

	const V2_int& GetSize() const {
		return size;
	}

	int GetLength() const {
		return length;
	}

	/// @return nullopt if coordinate is invalid, otherwise: coordinate.x + coordinate.y * size.x.
	[[nodiscard]] std::optional<int> OneDimensionalize(V2_int coordinate) const {
		if (coordinate.x < 0 || coordinate.y < 0) {
			return std::nullopt;
		}
		if (coordinate.x >= size.x || coordinate.y >= size.y) {
			return std::nullopt;
		}
		return coordinate.x + coordinate.y * size.x;
	}

	[[nodiscard]] V2_int TwoDimensionalize(int index) const {
		return V2_int{ index % size.x, index / size.x };
	}

	void Fill(const T& object) {
		static_assert(
			std::is_copy_constructible_v<T>,
			"Cannot fill grid with type which is not copy constructible"
		);
		std::ranges::fill(cells, object);
	}

protected:
	V2_int size;
	int length{ 0 };
	std::vector<T> cells;
};

} // namespace ptgn