#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <vector>

#include "debug/runtime/assert.h"
#include "math/vector2.h"

// TODO: Add serialization.

namespace ptgn {

template <typename T>
	requires std::is_default_constructible_v<T> && std::is_move_constructible_v<T>
class Grid {
public:
	Grid() = default;

	explicit Grid(const Vector2<int>& grid_dimensions, const std::vector<T>& grid_cells) :
		size{ grid_dimensions },
		length{ grid_dimensions.x * grid_dimensions.y },
		cells{ grid_cells } {
		PTGN_ASSERT(static_cast<std::size_t>(length) == cells.size(), "Failed to construct grid");
	}

	explicit Grid(const Vector2<int>& grid_dimensions) :
		size{ grid_dimensions },
		length{ grid_dimensions.x * grid_dimensions.y },
		cells(static_cast<std::size_t>(length)) {
		PTGN_ASSERT(static_cast<std::size_t>(length) == cells.size(), "Failed to construct grid");
	}

	void ForEachCoordinate(const std::function<void(V2_int)>& function) const {
		for (int i{ 0 }; i < size.x; i++) {
			for (int j{ 0 }; j < size.y; j++) {
				function(V2_int{ i, j });
			}
		}
	}

	void ForEach(const std::function<void(V2_int, const T&)>& function) const {
		for (int i{ 0 }; i < size.x; i++) {
			for (int j{ 0 }; j < size.y; j++) {
				V2_int coordinate{ i, j };
				function(coordinate, Get(coordinate));
			}
		}
	}

	void ForEach(const std::function<void(V2_int, T&)>& function) {
		for (int i{ 0 }; i < size.x; i++) {
			for (int j{ 0 }; j < size.y; j++) {
				V2_int coordinate{ i, j };
				function(coordinate, Get(coordinate));
			}
		}
	}

	void ForEachIndex(const std::function<void(int)>& function) const {
		for (int i{ 0 }; i < length; i++) {
			function(i);
		}
	}

	void ForEachElement(const std::function<void(T&)>& function) {
		for (auto& cell : cells) {
			function(cell);
		}
	}

	void ForEachElement(const std::function<void(const T&)>& function) const {
		for (auto& cell : cells) {
			function(cell);
		}
	}

	[[nodiscard]] bool Has(const V2_int& coordinate) const {
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

	[[nodiscard]] const T& Get(const V2_int& coordinate) const {
		return Get(OneDimensionalize(coordinate));
	}

	[[nodiscard]] T Pop(const V2_int& coordinate) {
		return Pop(OneDimensionalize(coordinate));
	}

	[[nodiscard]] T& Get(const V2_int& coordinate) {
		return Get(OneDimensionalize(coordinate));
	}

	[[nodiscard]] T Pop(int index) {
		PTGN_ASSERT(Has(index), "Cannot pop grid element which is outside the grid");
		T popped{ std::move(cells[static_cast<std::size_t>(index)]) };
		cells[static_cast<std::size_t>(index)] = T{};
		return popped;
	}

	[[nodiscard]] const T& Get(int index) const {
		PTGN_ASSERT(Has(index), "Cannot get grid element which is outside the grid");
		return cells[static_cast<std::size_t>(index)];
	}

	[[nodiscard]] T& Get(int index) {
		return const_cast<T&>(std::as_const(*this).Get(index));
	}

	T& Set(const V2_int& coordinate, T&& object) {
		return Set(OneDimensionalize(coordinate), std::move(object));
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

	[[nodiscard]] const V2_int& GetSize() const {
		return size;
	}

	[[nodiscard]] int GetLength() const {
		return length;
	}

	// @return -1 if coordinate is invalid, otherwise: coordinate.x + coordinate.y * size.x.
	[[nodiscard]] int OneDimensionalize(const V2_int& coordinate) const {
		if (coordinate.x < 0 || coordinate.y < 0) {
			return -1;
		}
		if (coordinate.x >= size.x || coordinate.y >= size.y) {
			return -1;
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
		std::fill(cells.begin(), cells.end(), object);
	}

protected:
	V2_int size;
	int length{ 0 };
	std::vector<T> cells;
};

} // namespace ptgn