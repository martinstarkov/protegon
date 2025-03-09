#include "serialization/file_stream_reader.h"

#include <cstdint>
#include <iosfwd>

#include "utility/file.h"

namespace ptgn {

FileStreamReader::FileStreamReader(const path& filename) :
	stream_{ std::ifstream(filename, std::ifstream::in | std::ifstream::binary) } {}

FileStreamReader::~FileStreamReader() {
	stream_.close();
}

bool FileStreamReader::IsStreamGood() const {
	return stream_.good();
}

std::uint64_t FileStreamReader::GetStreamPosition() {
	return stream_.tellg();
}

void FileStreamReader::SetStreamPosition(std::uint64_t position) {
	stream_.seekg(position);
}

void FileStreamReader::ReadData(char* destination, std::size_t size) {
	stream_.read(destination, size);
}

} // namespace ptgn