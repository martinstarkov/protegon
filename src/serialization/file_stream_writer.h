#pragma once

#include <cstdint>
#include <iosfwd>

#include "serialization/stream_writer.h"
#include "utility/file.h"

namespace ptgn {

class FileStreamWriter : public StreamWriter {
public:
	FileStreamWriter() = delete;
	explicit FileStreamWriter(const path& filename);
	FileStreamWriter& operator=(FileStreamWriter&&) noexcept = default;
	FileStreamWriter(FileStreamWriter&&) noexcept			 = default;
	FileStreamWriter& operator=(const FileStreamWriter&)	 = delete;
	FileStreamWriter(const FileStreamWriter&)				 = delete;
	~FileStreamWriter() final;

	bool IsStreamGood() const final;

	std::uint64_t GetStreamPosition() final;

	void SetStreamPosition(std::uint64_t position) final;

	void WriteData(const char* data, std::size_t size) final;

private:
	std::ofstream stream_;
};

} // namespace ptgn