#pragma once

#include <functional>
#include <type_traits>
#include <vector>

#include "math/vector2.h"
#include "utility/assert.h"
#include "utility/type_traits.h"

namespace ptgn {

template <typename T>
class Grid {
	static_assert(std::is_default_constructible_v<T>);
	static_assert(std::is_move_constructible_v<T>);

public:
	Grid() = default;

	explicit Grid(const Vector2<int>& size, const std::vector<T>& cells) :
		size{ size }, length{ size.x * size.y }, cells{ cells } {
		PTGN_ASSERT(static_cast<std::size_t>(length) == cells.size(), "Failed to construct grid");
	}

	explicit Grid(const Vector2<int>& size) :
		size{ size }, length{ size.x * size.y }, cells(static_cast<std::size_t>(length), T{}) {
		PTGN_ASSERT(static_cast<std::size_t>(length) == cells.size(), "Failed to construct grid");
	}

	void ForEachCoordinate(const std::function<void(V2_int)>& function) const {
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				std::invoke(function, V2_int{ i, j });
			}
		}
	}

	void ForEach(const std::function<void(V2_int, const T&)>& function) const {
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				V2_int coordinate{ i, j };
				std::invoke(function, coordinate, Get(coordinate));
			}
		}
	}

	void ForEach(const std::function<void(V2_int, T&)>& function) {
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				V2_int coordinate{ i, j };
				std::invoke(function, coordinate, Get(coordinate));
			}
		}
	}

	void ForEachIndex(const std::function<void(int)>& function) const {
		for (int i = 0; i < length; i++) {
			std::invoke(function, i);
		}
	}

	void ForEachElement(const std::function<void(T&)>& function) {
		for (auto& cell : cells) {
			std::invoke(function, cell);
		}
	}

	void ForEachElement(const std::function<void(const T&)>& function) const {
		for (auto& cell : cells) {
			std::invoke(function, cell);
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

	T& Set(const V2_int& coordinate, T&& object) {
		return Set(OneDimensionalize(coordinate), std::move(object));
	}

	[[nodiscard]] const T& Get(const V2_int& coordinate) const {
		return Get(OneDimensionalize(coordinate));
	}

	[[nodiscard]] T& Get(const V2_int& coordinate) {
		return Get(OneDimensionalize(coordinate));
	}

	[[nodiscard]] const T& Get(int index) const {
		PTGN_ASSERT(Has(index), "Cannot get grid element which is outside the grid");
		return cells[static_cast<std::size_t>(index)];
	}

	[[nodiscard]] T& Get(int index) {
		PTGN_ASSERT(Has(index), "Cannot get grid element which is outside the grid");
		return cells[static_cast<std::size_t>(index)];
	}

	T& Set(int index, T&& object) {
		PTGN_ASSERT(Has(index), "Cannot set grid element which is outside the grid");
		auto& value = cells[static_cast<std::size_t>(index)];
		value		= std::move(object);
		return value;
	}

	[[nodiscard]] bool Has(int index) const {
		return index >= 0 && index < length;
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

	template <tt::copy_constructible<T> = true>
	void Fill(const T& object) {
		std::fill(cells.begin(), cells.end(), object);
	}

protected:
	V2_int size;
	int length{ 0 };
	std::vector<T> cells;
};

} // namespace ptgn