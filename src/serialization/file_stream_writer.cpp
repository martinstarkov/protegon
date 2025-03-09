#include "serialization/file_stream_writer.h"

#include <cstdint>
#include <iosfwd>

#include "utility/file.h"

namespace ptgn {

FileStreamWriter::FileStreamWriter(const path& filename) :
	stream_{ std::ofstream(filename, std::ifstream::out | std::ifstream::binary) } {}

FileStreamWriter::~FileStreamWriter() {
	stream_.close();
}

bool FileStreamWriter::IsStreamGood() const {
	return stream_.good();
}

std::uint64_t FileStreamWriter::GetStreamPosition() {
	return stream_.tellp();
}

void FileStreamWriter::SetStreamPosition(std::uint64_t position) {
	stream_.seekp(position);
}

void FileStreamWriter::WriteData(const char* data, std::size_t size) {
	stream_.write(data, size);
}

} // namespace ptgn