#pragma once

#include <concepts>
#include <limits>
#include <vector>

namespace ptgn {

template <std::integral I, typename T>
struct IdMap {
	void Add(I id, T&& c) {
		if (id >= sparse_.size()) {
			sparse_.resize(id + 1, std::numeric_limits<I>::max());
		}

		if (sparse_[id] == std::numeric_limits<I>::max()) { // new
			sparse_[id] = dense_.size();
			dense_.emplace_back(id);
			data_.emplace_back(c);
		} else { // overwrite
			data_[sparse_[id]] = c;
		}
	}

	void Remove(I id) {
		if (id >= sparse_.size() || sparse_[id] == std::numeric_limits<I>::max()) {
			return;
		}

		auto idx  = sparse_[id];
		auto last = dense_.back();

		dense_[idx] = last;
		data_[idx]	= data_.back();

		sparse_[last] = idx;
		sparse_[id]	  = std::numeric_limits<I>::max();

		dense_.pop_back();
		data_.pop_back();
	}

	bool Has(I id) const {
		return id < sparse_.size() && sparse_[id] != std::numeric_limits<I>::max();
	}

	const T* TryGet(I id) const {
		return Has(id) ? &data_[sparse_[id]] : nullptr;
	}

	T* TryGet(I id) {
		return Has(id) ? &data_[sparse_[id]] : nullptr;
	}

	const T& Get(I id) const {
		PTGN_ASSERT(Has(id));
		return data_[sparse_[id]];
	}

	T& Get(I id) {
		PTGN_ASSERT(Has(id));
		return data_[sparse_[id]];
	}

private:
	std::vector<std::size_t> dense_;
	std::vector<I> sparse_;
	std::vector<T> data_;
};

} // namespace ptgn