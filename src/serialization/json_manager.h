#pragma once

#include <fstream>
#include <string_view>
#include <unordered_map>

#include "serialization/json.h"
#include "utility/file.h"

namespace ptgn::impl {

class JsonManager {
public:
	void Load(std::string_view key, const path& filepath);

	void Unload(std::string_view key);

	[[nodiscard]] const json& Get(std::string_view key) const;

private:
	[[nodiscard]] bool Has(std::size_t key) const;

	std::unordered_map<std::size_t, json> jsons_;
};

} // namespace ptgn::impl