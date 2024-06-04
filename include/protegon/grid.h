#pragma once

#include "vector2.h"
#include "type_traits.h"

namespace ptgn {

template <typename T>
class Grid {
	static_assert(std::is_default_constructible_v<T>);
	static_assert(std::is_move_constructible_v<T>);
public:
	Grid() = delete;

	Grid(const Vector2<int>& size, const std::vector<T>& cells) : size{ size }, length{ size.x * size.y }, cells{ cells } {
		assert(length == cells.size() && "Failed to construct grid");
	}

	Grid(const Vector2<int>& size) : size{ size }, length{ size.x * size.y }, cells(length, T{}) {
		assert(length == cells.size() && "Failed to construct grid");
	}

	void ForEachCoordinate(std::function<void(V2_int)> function) {
		for (int i = 0; i < size.x; i++)
			for (int j = 0; j < size.y; j++)
				function(V2_int{ i, j });
	}

	void ForEachIndex(std::function<void(std::size_t)> function) {
		for (std::size_t i = 0; i < length; i++)
			function(i);
	}

	void ForEachElement(std::function<void(T&)> function) {
		for (auto& cell : cells)
			function(cell);
	}

	bool Has(const V2_int& coordinate) const {
		return Has(OneDimensionalize(coordinate));
	}

	T& Set(const V2_int& coordinate, T&& object) {
		return Set(OneDimensionalize(coordinate), std::move(object));
	}

	const T& Get(const V2_int& coordinate) const {
		return Get(OneDimensionalize(coordinate));
	}

	T& Get(const V2_int& coordinate) {
		return Get(OneDimensionalize(coordinate));
	}

	const T& Get(std::size_t index) const {
		assert(Has(index) && "Cannot get grid element which is outside the grid");
		return cells[index];
	}

	T& Get(std::size_t index) {
		assert(Has(index) && "Cannot get grid element which is outside the grid");
		return cells[index];
	}

	T& Set(std::size_t index, T&& object) {
		assert(Has(index) && "Cannot set grid element which is outside the grid");
		auto& value = cells[index];
		value = std::move(object);
		return value;
	}

	bool Has(std::size_t index) const {
		return index >= 0 && index < length;
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

	int OneDimensionalize(const V2_int& coordinate) const {
		return coordinate.x + coordinate.y * size.x;
	}

	template <typename T, type_traits::copy_constructible<T> = true>
	void Fill(const T& object) {
		std::fill(cells.begin(), cells.end(), object);
	}
protected:
	int length{ 0 };
	V2_int size;
	std::vector<T> cells;
};

} // namespace ptgn