#pragma once

#include <cstdint>
#include <iosfwd>

#include "serialization/stream_reader.h"
#include "utility/file.h"

namespace ptgn {

class FileStreamReader : public StreamReader {
public:
	FileStreamReader() = delete;
	explicit FileStreamReader(const path& filename);
	FileStreamReader& operator=(FileStreamReader&&) noexcept = default;
	FileStreamReader(FileStreamReader&&) noexcept			 = default;
	FileStreamReader& operator=(const FileStreamReader&)	 = delete;
	FileStreamReader(const FileStreamReader&)				 = delete;
	~FileStreamReader() final;

	bool IsStreamGood() const final;

	std::uint64_t GetStreamPosition() final;

	void SetStreamPosition(std::uint64_t position) final;

	void ReadData(char* destination, std::size_t size) final;

private:
	std::ifstream stream_;
};

} // namespace ptgn