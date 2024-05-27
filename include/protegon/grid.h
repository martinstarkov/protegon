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
	
	void ForEachCoordinate(std::function<void(V2_int)> function);
	void ForEachIndex(std::function<void(std::size_t)> function);
	void ForEachElement(std::function<void(T&)> function);

	template <typename T, type_traits::copy_constructible<T> = true>
	void Fill(const T& object) {
		std::fill(cells.begin(), cells.end(), object);
	}

	bool Has(const V2_int& coordinate) const;
	T& Set(const V2_int& coordinate, T&& object);
	const T& Get(const V2_int& coordinate) const;
	T& Get(const V2_int& coordinate);

	bool Has(std::size_t index) const;
	T& Set(std::size_t, T&& object);
	const T& Get(std::size_t index) const;
	T& Get(std::size_t index);

	void Clear();
	const V2_int& GetSize() const;
	int GetLength() const;

	int OneDimensionalize(const V2_int& coordinate) const;
protected:
	int length{ 0 };
	V2_int size;
	std::vector<T> cells;
};

} // namespace ptgn