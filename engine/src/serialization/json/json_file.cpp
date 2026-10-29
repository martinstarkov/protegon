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

void SaveJson(const json& value, const path& file_path, int indent) {
    indent = std::max(indent, 0);

	EnsureDirectory(file_path.parent_path());

	// This must happen before touching the existing file.
	std::string serialized{
		value.dump(4)
	};

	std::ofstream output{
		file_path,
		std::ios::binary | std::ios::trunc
	};

	if (indent) {
		output << std::setw(indent);
	}

	PTGN_ASSERT(
		output,
		"Failed to open JSON file for writing: ",
		file_path.string()
	);

	output.write(
		serialized.data(),
		static_cast<std::streamsize>(serialized.size())
	);
	output.put('\n');

	PTGN_ASSERT(
		output,
		"Failed to write JSON file: ",
		file_path.string()
	);
}

} // namespace ptgn