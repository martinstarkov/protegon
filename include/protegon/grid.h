#pragma once

#include "vector2.h"
#include "type_traits.h"

namespace ptgn {

template <typename T>
class Grid {
public:
	Grid() = delete;
	Grid(const Vector2<int>& size, const std::unordered_map<V2_int, T>& cells) : size{ size }, cells{ cells } {}
	Grid(const Vector2<int>& size) : size{ size } {
		Fill({});
	}
	bool InBound(const V2_int& coordinate) const {
		return coordinate.x < size.x && coordinate.x >= 0 &&
			   coordinate.y < size.y && coordinate.y >= 0;
	}
	template <typename L>
	void ForEach(L function) {
		for (int i = 0; i < size.x; i++)
			for (int j = 0; j < size.y; j++)
				function(i, j);
	}
	template <typename U = T, type_traits::copy_constructible<U> = true>
	void Fill(const T& object) {
		ForEach([&](int i, int j) {
			cells.emplace(V2_int{ i, j }, object);
		});	
	}
	T& Insert(const V2_int& coordinate, const T&& object) {
		assert(InBound(coordinate));
		return cells.insert_or_assign(coordinate, std::move(object))->first->second;
	}
	void Remove(const V2_int& coordinate) {
		assert(InBound(coordinate));
		auto it = cells.find(coordinate);
		if (it != cells.end()) {
			cells.erase(it);
		}
	}
	bool Has(const V2_int& coordinate) const {
		assert(InBound(coordinate));
		return cells.find(coordinate) != cells.end();
	}
	Grid<T> GetSubgridWith(const T& object) {
		std::unordered_map<V2_int, T> cells_with;
		cells_with.reserve(cells.size());
		for (auto& [key, value] : cells)
			if (value == object)
				cells_with.emplace(key, value);
		return { size, cells_with };
	}
	Grid<T> GetSubgridWithout(const T& object) {
		std::unordered_map<V2_int, T> cells_without;
		cells_without.reserve(cells.size());
		for (auto& [key, value] : cells)
			if (value != object)
				cells_without.emplace(key, value);
		return { size, cells_without };
	}
	const T& Get(const V2_int& coordinate) const {
		assert(InBound(coordinate));
		auto it = cells.find(coordinate);
		assert(it != cells.end());
		return it->second;
	}
	T& Get(const V2_int& coordinate) {
		assert(InBound(coordinate));
		auto it = cells.find(coordinate);
		assert(it != cells.end());
		return it->second;
	}
	const V2_int& GetSize() const {
		return size;
	}
	void Clear() {
		cells.clear();
	}
	std::unordered_map<V2_int, T> cells;
private:
	V2_int size;
};

} // namespace ptgn