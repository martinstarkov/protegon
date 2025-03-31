#include "serialization/binary_archive.h"

#include <cstdint>
#include <iosfwd>

#include "utility/file.h"

namespace ptgn {

BinaryInputArchive::BinaryInputArchive(const path& filename) :
	stream_{ std::ifstream(filename, std::ifstream::in | std::ifstream::binary) } {
	PTGN_CHECK(stream_.is_open(), "Failed to open binary file for reading: " + filename.string());
}

BinaryInputArchive::~BinaryInputArchive() {
	stream_.close();
}

bool BinaryInputArchive::IsStreamGood() const {
	return stream_.good();
}

std::uint64_t BinaryInputArchive::GetStreamPosition() {
	return stream_.tellg();
}

void BinaryInputArchive::SetStreamPosition(std::uint64_t position) {
	stream_.seekg(position);
}

void BinaryInputArchive::ReadData(char* destination, std::size_t size) {
	stream_.read(destination, size);
}

BinaryInputArchive::operator bool() const {
	return IsStreamGood();
}

BinaryOutputArchive::BinaryOutputArchive(const path& filename) :
	stream_{ std::ofstream(filename, std::ifstream::out | std::ifstream::binary) } {
	PTGN_CHECK(stream_.is_open(), "Failed to open binary file for writing: " + filename.string());
}

BinaryOutputArchive::~BinaryOutputArchive() {
	stream_.close();
}

bool BinaryOutputArchive::IsStreamGood() const {
	return stream_.good();
}

std::uint64_t BinaryOutputArchive::GetStreamPosition() {
	return stream_.tellp();
}

void BinaryOutputArchive::SetStreamPosition(std::uint64_t position) {
	stream_.seekp(position);
}

void BinaryOutputArchive::WriteData(const char* data, std::size_t size) {
	stream_.write(data, size);
}

BinaryOutputArchive::operator bool() const {
	return IsStreamGood();
}

void BinaryOutputArchive::WriteZeroByte(std::size_t count) {
	char zero{ 0 };
	for (std::size_t i{ 0 }; i < count; i++) {
		WriteData(&zero, 1);
	}
}

} // namespace ptgn