#pragma once

namespace ptgn {

// TODO: Implement a stat registration map.
class Stats {
public:
	void Reset() {
		counts_.clear();
	}

	void Increment(std::string_view stat_name, std::size_t amount = 1) {
		counts_[stat_name] += amount;
	}

	std::size_t GetCount(std::string_view stat_name) const {
		auto it = counts_.find(stat_name);
		return (it != counts_.end()) ? it->second : 0;
	}

private:
	std::unordered_map<std::string_view, std::size_t, StringHash, std::equal_to<>> counts_;
};

} // namespace ptgn