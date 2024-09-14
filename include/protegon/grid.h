#pragma once

#include <functional>
#include <type_traits>
#include <vector>

#include "protegon/vector2.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

template <typename T>
class Grid {
	static_assert(std::is_default_constructible_v<T>);
	static_assert(std::is_move_constructible_v<T>);

public:
	Grid() = delete;

	explicit Grid(const Vector2<int>& size, const std::vector<T>& cells) :
		size{ size },
		length{ static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) },
		cells{ cells } {
		PTGN_ASSERT(length == cells.size(), "Failed to construct grid");
	}

	explicit Grid(const Vector2<int>& size) :
		size{ size }, length{ size.x * size.y }, cells(length, T{}) {
		PTGN_ASSERT(length == cells.size(), "Failed to construct grid");
	}

	void ForEachCoordinate(const std::function<void(V2_int)>& function) const {
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				std::invoke(function, V2_int{ i, j });
			}
		}
	}

	void ForEachIndex(const std::function<void(std::size_t)>& function) const {
		for (std::size_t i = 0; i < length; i++) {
			std::invoke(function, i);
		}
	}

	void ForEachElement(const std::function<void(T&)>& function) {
		for (auto& cell : cells) {
			std::invoke(function, cell);
		}
	}

	[[nodiscard]] bool Has(const V2_int& coordinate) const {
		return Has(OneDimensionalize(coordinate));
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

	[[nodiscard]] const T& Get(std::size_t index) const {
		PTGN_ASSERT(Has(index), "Cannot get grid element which is outside the grid");
		return cells[index];
	}

	[[nodiscard]] T& Get(std::size_t index) {
		PTGN_ASSERT(Has(index), "Cannot get grid element which is outside the grid");
		return cells[index];
	}

	T& Set(std::size_t index, T&& object) {
		PTGN_ASSERT(Has(index), "Cannot set grid element which is outside the grid");
		auto& value = cells[index];
		value		= std::move(object);
		return value;
	}

	[[nodiscard]] bool Has(std::size_t index) const {
		return index >= 0 && index < length;
	}

	void Clear() {
		cells.clear();
	}

	[[nodiscard]] const V2_int& GetSize() const {
		return size;
	}

	[[nodiscard]] std::size_t GetLength() const {
		return length;
	}

	[[nodiscard]] int OneDimensionalize(const V2_int& coordinate) const {
		return coordinate.x + coordinate.y * size.x;
	}

	template <tt::copy_constructible<T> = true>
	void Fill(const T& object) {
		std::fill(cells.begin(), cells.end(), object);
	}

protected:
	std::size_t length{ 0 };
	V2_int size;
	std::vector<T> cells;
};

} // namespace ptgn