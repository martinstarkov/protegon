#pragma once

#include "vector2.h"
#include "type_traits.h"

namespace ptgn {

template <typename T>
struct Grid {
	Grid() = delete;
	Grid(const Vector2<int>& size) : size{ size } {}
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
	void Insert(const V2_int& coordinate, const T&& object) {
		assert(InBound(coordinate));
		cells.insert_or_assign(coordinate, std::move(object));
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
	const T& GetTile(const V2_int& coordinate) const {
		assert(InBound(coordinate));
		auto it = cells.find(coordinate);
		assert(it != cells.end());
		return it->second;
	}
	T& GetTile(const V2_int& coordinate) {
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
private:
	V2_int size;
	std::unordered_map<V2_int, T> cells;
};

} // namespace ptgn