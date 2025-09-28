#include "serialization/json.h"

#include <filesystem>
#include <iomanip>
#include <iosfwd>
#include <nlohmann/json.hpp>
#include <ostream>

#include "common/assert.h"
#include "debug/log.h"
#include "serialization/fwd.h"
#include "utility/file.h"

namespace ptgn {

void SaveJson(const json& j, const path& filepath, bool indent) {
	std::ofstream o{ filepath };
	if (indent) {
		o << std::setw(4);
	}
	o << j << std::endl;
}

json LoadJson(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot load json file from a nonexistent file path: ", filepath.string()
	);
	std::ifstream json_file(filepath);
	json j = json::parse(json_file);
	return j;
}

} // namespace ptgn