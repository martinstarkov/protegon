#include "protegon/grid.h"

namespace ptgn {

template <typename T>
void Grid<T>::ForEachCoordinate(std::function<void(V2_int)> function) {
	for (int i = 0; i < size.x; i++)
		for (int j = 0; j < size.y; j++)
			function(V2_int{ i, j });
}

template <typename T>
void Grid<T>::ForEachIndex(std::function<void(std::size_t)> function) {
	for (std::size_t i = 0; i < length; i++)
		function(i);
}

template <typename T>
void Grid<T>::ForEachElement(std::function<void(T&)> function) {
	for (auto& cell : cells)
		function(cell);
}

template <typename T>
bool Grid<T>::Has(const V2_int& coordinate) const {
	return Has(OneDimensionalize(coordinate));
}

template <typename T>
T& Grid<T>::Set(const V2_int& coordinate, T&& object) {
	return Set(OneDimensionalize(coordinate), std::move(object));
}

template <typename T>
const T& Grid<T>::Get(const V2_int& coordinate) const {
	return Get(OneDimensionalize(coordinate));
}

template <typename T>
T& Grid<T>::Get(const V2_int& coordinate) {
	return Get(OneDimensionalize(coordinate));
}

template <typename T>
const T& Grid<T>::Get(std::size_t index) const {
	assert(Has(index) && "Cannot get grid element which is outside the grid");
	return cells[index];
}

template <typename T>
T& Grid<T>::Get(std::size_t index) {
	assert(Has(index) && "Cannot get grid element which is outside the grid");
	return cells[index];
}

template <typename T>
T& Grid<T>::Set(std::size_t index, T&& object) {
	assert(Has(index) && "Cannot set grid element which is outside the grid");
	auto& value = cells[index];
	value = std::move(object);
	return value;
}

template <typename T>
bool Grid<T>::Has(std::size_t index) const {
	return index >= 0 && index < length;
}

template <typename T>
void Grid<T>::Clear() {
	cells.clear();
}

template <typename T>
const V2_int& Grid<T>::GetSize() const {
	return size;
}

template <typename T>
int Grid<T>::GetLength() const {
	return length;
}

template <typename T>
int Grid<T>::OneDimensionalize(const V2_int& coordinate) const {
	return coordinate.x + coordinate.y * size.x;
}

} // namespace ptgn