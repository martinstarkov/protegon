#pragma once

#include <fstream>

#include "core/manager.h"
#include "serialization/json.h"
#include "utility/debug.h"
#include "utility/file.h"

namespace ptgn {

namespace impl {

class JsonManager : public MapManager<json> {
public:
	JsonManager()								   = default;
	virtual ~JsonManager()						   = default;
	JsonManager(JsonManager&&) noexcept			   = default;
	JsonManager& operator=(JsonManager&&) noexcept = default;
	JsonManager(const JsonManager&)				   = delete;
	JsonManager& operator=(const JsonManager&)	   = delete;

	/*
	 * If key already exists, does nothing.
	 * @param key Unique id of the json file to be loaded.
	 * @param path File system path for the json file.
	 * @return Reference to the loaded item.
	 */
	template <typename TKey>
	json& Load(const TKey& key, const path& json_path) {
		PTGN_ASSERT(
			FileExists(json_path),
			"Cannot create json file from a nonexistent file path: ", json_path.string()
		);
		std::ifstream json_file(json_path);
		return MapManager<json>::Load(key, std::move(json::parse(json_file)));
	}
};

} // namespace impl

} // namespace ptgn