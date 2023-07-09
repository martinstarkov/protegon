#pragma once

#include "vector2.h"
#include "type_traits.h"

namespace ptgn {

template <typename T>
class Grid {
	static_assert(std::is_default_constructible_v<T>);
public:
	Grid() = delete;
	Grid(const Vector2<int>& size, const std::vector<T>& cells) : size{ size }, length{ size.x * size.y }, cells{ cells } {
		assert(length == cells.size());
	}
	Grid(const Vector2<int>& size) : size{ size }, length{ size.x * size.y }, cells(length, T{}) {
		assert(length == cells.size());
	}
	template <typename L>
	void ForEachCoordinate(L function) {
		for (int i = 0; i < size.x; i++)
			for (int j = 0; j < size.y; j++)
				function(V2_int{ i, j });
	}
	template <typename L>
	void ForEachIndex(L function) {
		for (int i = 0; i < length; i++)
			function(i);
	}
	template <typename L>
	void ForEachElement(L function) {
		for (auto& cell : cells) {
			function(cell);
		}
	}
	template <typename U = T, type_traits::copy_constructible<U> = true>
	void Fill(const T& object) {
		std::fill(cells.begin(), cells.end(), object);
	}
	bool Has(const V2_int& coordinate) const {
		return coordinate.x >= 0 &&
			   coordinate.y >= 0 &&
			   coordinate.x < size.x &&
			   coordinate.y < size.y;
	}
	T* Set(const V2_int& coordinate, T&& object) {
		assert(Has(coordinate));
		const auto point{ OneDimensionalize(coordinate) };
		cells[point] = std::move(object);
		return &cells[point];
	}
	const T* Get(const V2_int& coordinate) const {
		assert(Has(coordinate));
		const auto point{ OneDimensionalize(coordinate) };
		return &cells[point];
	}
	T* Get(const V2_int& coordinate) {
		assert(Has(coordinate));
		const auto point{ OneDimensionalize(coordinate) };
		return &cells[point];
	}
	const T* Get(std::size_t index) const {
		assert(index >= 0 && index < length);
		return &cells[index];
	}
	T* Get(std::size_t index) {
		assert(index >= 0 && index < length);
		return &cells[index];
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
private:
	V2_int size;
	int length{ 0 };
	int OneDimensionalize(const V2_int& coordinate) const {
		return coordinate.x + coordinate.y * size.x;
	}
protected:
	std::vector<T> cells;
};

} // namespace ptgn