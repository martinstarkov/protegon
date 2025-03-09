#pragma once

#include <cstdint>
#include <type_traits>

#include "utility/type_traits.h"

namespace ptgn {

class StreamWriter {
public:
	template <typename T>
	static constexpr bool is_stream_serializable_type_v{
		tt::is_string_like_v<T> || tt::is_map_like_v<T> || tt::is_std_array_v<T> ||
		tt::is_std_vector_v<T> || tt::is_serializable_v<T, StreamWriter> ||
		std::is_trivially_copyable_v<T>
	};

	virtual ~StreamWriter() = default;

	virtual bool IsStreamGood() const = 0;

	virtual std::uint64_t GetStreamPosition() = 0;

	virtual void SetStreamPosition(std::uint64_t position) = 0;

	virtual void WriteData(const char* data, std::size_t size) = 0;

	operator bool() const {
		return IsStreamGood();
	}

	// Useful for inserting unknown values and then later populating them by storing their stream
	// positions.
	// @param count The number of zero bytes to write.
	void WriteZeroByte(std::size_t count) {
		char zero{ 0 };
		for (std::size_t i{ 0 }; i < count; i++) {
			WriteData(&zero, 1);
		}
	}

	template <typename T, tt::enable<is_stream_serializable_type_v<T>> = true>
	void Write(const T& type) {
		if constexpr (tt::is_serializable_v<T, StreamWriter>) {
			T::Serialize(this, type);
		} else if constexpr (tt::is_map_like_v<T>) {
			WriteRaw(type.size());

			for (const auto& [key, value] : type) {
				Write(key);
				Write(value);
			}
		} else if constexpr (tt::is_std_array_v<T> || tt::is_std_vector_v<T>) {
			WriteRaw(type.size());

			for (const auto& value : type) {
				Write(value);
			}
		} else if constexpr (tt::is_string_like_v<T>) {
			auto size{ type.size() };
			WriteRaw(size);
			WriteBuffer(type.data(), sizeof(char) * size);
		} else if constexpr (std::is_trivially_copyable_v<T>) {
			WriteRaw(type);
		} else {
			static_assert(false, "StreamWriter::is_stream_serializable_type_v must be updated");
		}
	}

	template <typename T>
	void WriteBuffer(const T* buffer, std::size_t size) {
		WriteData(reinterpret_cast<const char*>(buffer), size);
	}

	template <typename T>
	void WriteRaw(const T& type) {
		WriteData(reinterpret_cast<const char*>(&type), sizeof(T));
	}
};

} // namespace ptgn