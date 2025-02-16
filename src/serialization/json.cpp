#include "serialization/json.h"

#include <filesystem>
#include <iosfwd>
#include <nlohmann/json.hpp>

#include "serialization/fwd.h"
#include "utility/assert.h"
#include "utility/file.h"

namespace ptgn {

json LoadJson(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot load json file from a nonexistent file path: ", filepath.string()
	);
	std::ifstream json_file(filepath);
	return json::parse(json_file);
}

} // namespace ptgn