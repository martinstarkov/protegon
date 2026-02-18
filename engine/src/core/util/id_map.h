#pragma once

#include <concepts>
#include <limits>
#include <vector>

#include "core/assert.h"

namespace ptgn {

template <typename T>
struct IdMap {
	[[nodiscard]] std::size_t Size() const {
		return dense_.size();
	}

	[[nodiscard]] bool IsEmpty() const {
		return !Size();
	}

	void Clear() {
		dense_.clear();
		sparse_.clear();
		data_.clear();
	}

	void Add(std::size_t id, T&& value) {
		if (id >= sparse_.size()) {
			sparse_.resize(id + 1, std::numeric_limits<std::size_t>::max());
		}

		if (sparse_[id] == std::numeric_limits<std::size_t>::max()) { // new
			sparse_[id] = dense_.size();
			dense_.emplace_back(id);
			data_.emplace_back(value);
		} else { // overwrite
			data_[sparse_[id]] = value;
		}
	}

	void Add(std::size_t id, const T& value) {
		return Add(id, T{ value });
	}

	void Remove(std::size_t id) {
		if (id >= sparse_.size() || sparse_[id] == std::numeric_limits<std::size_t>::max()) {
			return;
		}

		auto idx  = sparse_[id];
		auto last = dense_.back();

		dense_[idx] = last;
		data_[idx]	= data_.back();

		sparse_[last] = idx;
		sparse_[id]	  = std::numeric_limits<std::size_t>::max();

		dense_.pop_back();
		data_.pop_back();
	}

	bool Has(std::size_t id) const {
		return id < sparse_.size() && sparse_[id] != std::numeric_limits<std::size_t>::max();
	}

	auto Find(std::size_t id) {
		return Has(id) ? data_.begin() + sparse_[id] : data_.end();
	}

	auto Find(std::size_t id) const {
		return Has(id) ? data_.begin() + sparse_[id] : data_.end();
	}

	const T* TryGet(std::size_t id) const {
		return Has(id) ? &data_[sparse_[id]] : nullptr;
	}

	T* TryGet(std::size_t id) {
		return Has(id) ? &data_[sparse_[id]] : nullptr;
	}

	const T& Get(std::size_t id) const {
		PTGN_ASSERT(Has(id), "Id does not exist in the id map");
		return data_[sparse_[id]];
	}

	T& Get(std::size_t id) {
		PTGN_ASSERT(Has(id), "Id does not exist in the id map");
		return data_[sparse_[id]];
	}

	auto begin() {
		return data_.begin();
	}

	auto end() {
		return data_.end();
	}

	auto begin() const {
		return data_.begin();
	}

	auto end() const {
		return data_.end();
	}

	auto cbegin() const {
		return data_.cbegin();
	}

	auto cend() const {
		return data_.cend();
	}

private:
	std::vector<std::size_t> dense_;
	std::vector<std::size_t> sparse_;
	std::vector<T> data_;
};

} // namespace ptgn