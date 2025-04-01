#include "serialization/json_archive.h"

#include <cstdint>
#include <iosfwd>

#include "utility/file.h"

namespace ptgn {

JsonInputArchive::JsonInputArchive(const path& filepath) {
	std::ifstream stream{ filepath, std::ifstream::in };
	PTGN_CHECK(stream.is_open(), "Failed to open binary file for reading: " + filepath.string());
	stream >> data_;
}

JsonOutputArchive::JsonOutputArchive(const path& filepath) : filepath_{ filepath } {}

void JsonOutputArchive::WriteToFile() const {
	PTGN_ASSERT(!filepath_.empty(), "Cannot write to empty filepath");
	if (!data_.empty()) {
		std::ofstream stream{ filepath_, std::ifstream::out };
		PTGN_CHECK(
			stream.is_open(), "Failed to open binary file for writing: " + filepath_.string()
		);
		stream << data_.dump(4);
	}
}

JsonOutputArchive::~JsonOutputArchive() {
	if (!filepath_.empty()) {
		WriteToFile();
	}
}

} // namespace ptgn