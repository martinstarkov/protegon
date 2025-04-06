#include "serialization/json.h"

#include <filesystem>
#include <iomanip>
#include <iosfwd>
#include <nlohmann/json.hpp>
#include <ostream>

#include "serialization/fwd.h"
#include "utility/assert.h"
#include "utility/file.h"

namespace ptgn {

void SaveJson(const json& j, const path& filepath) {
	std::ofstream o{ filepath };
	o << std::setw(4) << j << std::endl;
}

json LoadJson(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot load json file from a nonexistent file path: ", filepath.string()
	);
	std::ifstream json_file(filepath);
	return json::parse(json_file);
}

} // namespace ptgn