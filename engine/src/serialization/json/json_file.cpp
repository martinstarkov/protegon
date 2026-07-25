#include "serialization/json/json_file.h"

#include <fstream>
#include <iomanip>

#include "core/util/file.h"
#include "serialization/json/json.h"

namespace ptgn {

json LoadJson(const path& file_path) {
	PTGN_ASSERT(
		FileExists(file_path),
		"Cannot load json file from a nonexistent file path: ", file_path.string()
	);
	std::ifstream json_file(file_path);
	json j = json::parse(json_file);
	return j;
}

void SaveJson(const json& j, const path& file_path, int indent) {
    indent = std::max(indent, 0);

	std::ofstream of{ file_path };

	if (indent) {
		of << std::setw(indent);
	}
    
	of << j << std::endl;
}

} // namespace ptgn