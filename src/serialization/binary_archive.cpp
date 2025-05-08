#include "serialization/binary_archive.h"

#include <cstdint>
#include <iosfwd>

#include "utility/file.h"

namespace ptgn {

BinaryInputArchive::BinaryInputArchive(const path& filepath) :
	stream_{ std::ifstream(filepath, std::ifstream::in | std::ifstream::binary) } {
	PTGN_CHECK(stream_.is_open(), "Failed to open binary file for reading: " + filepath.string());
}

BinaryInputArchive::~BinaryInputArchive() {
	stream_.close();
}

bool BinaryInputArchive::IsStreamGood() const {
	return stream_.good();
}

std::uint64_t BinaryInputArchive::GetStreamPosition() {
	auto pos{ stream_.tellg() };
	PTGN_ASSERT(pos >= 0, "Failed to retrieve a valid stream position");
	return static_cast<std::uint64_t>(pos);
}

void BinaryInputArchive::SetStreamPosition(std::uint64_t position) {
	stream_.seekg(static_cast<std::streamoff>(position));
}

void BinaryInputArchive::ReadData(char* destination, std::streamsize size) {
	stream_.read(destination, size);
}

BinaryInputArchive::operator bool() const {
	return IsStreamGood();
}

BinaryOutputArchive::BinaryOutputArchive(const path& filepath) :
	stream_{ std::ofstream(filepath, std::ifstream::out | std::ifstream::binary) } {
	PTGN_CHECK(stream_.is_open(), "Failed to open binary file for writing: " + filepath.string());
}

BinaryOutputArchive::~BinaryOutputArchive() {
	stream_.close();
}

bool BinaryOutputArchive::IsStreamGood() const {
	return stream_.good();
}

std::uint64_t BinaryOutputArchive::GetStreamPosition() {
	auto pos{ stream_.tellp() };
	PTGN_ASSERT(pos >= 0, "Failed to retrieve a valid stream position");
	return static_cast<std::uint64_t>(pos);
}

void BinaryOutputArchive::SetStreamPosition(std::uint64_t position) {
	stream_.seekp(static_cast<std::int64_t>(position));
}

void BinaryOutputArchive::WriteData(const char* data, std::streamsize size) {
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