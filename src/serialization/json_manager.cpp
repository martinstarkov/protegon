#include "serialization/json_manager.h"

#include <fstream>
#include <string_view>

#include "math/hash.h"
#include "utility/assert.h"

namespace ptgn::impl {

void JsonManager::Load(std::string_view key, const path& filepath) {
	jsons_.try_emplace(Hash(key), LoadFromFile(filepath));
}

void JsonManager::Unload(std::string_view key) {
	jsons_.erase(Hash(key));
}

const json& JsonManager::Get(std::string_view key) const {
	PTGN_ASSERT(Has(Hash(key)), "Cannot get json file which is not loaded");
	return jsons_.find(Hash(key))->second;
}

bool JsonManager::Has(std::size_t key) const {
	return jsons_.find(key) != jsons_.end();
}

json JsonManager::LoadFromFile(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create json file from a nonexistent file path: ", filepath.string()
	);
	std::ifstream json_file(filepath);
	return json::parse(json_file);
}

} // namespace ptgn::impl