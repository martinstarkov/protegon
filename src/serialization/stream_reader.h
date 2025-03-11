#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "ecs/ecs.h"
#include "utility/assert.h"
#include "utility/type_traits.h"

namespace ptgn {

class StreamReader {
public:
	template <typename T>
	static constexpr bool is_stream_deserializable_type_v{
		std::is_same_v<T, std::string> || tt::is_map_like_v<T> || tt::is_std_array_v<T> ||
		tt::is_std_vector_v<T> || tt::is_deserializable_v<T, StreamReader> ||
		std::is_trivially_copyable_v<T>
	};

	virtual ~StreamReader() = default;

	virtual bool IsStreamGood() const					   = 0;
	virtual std::uint64_t GetStreamPosition()			   = 0;
	virtual void SetStreamPosition(std::uint64_t position) = 0;
	virtual void ReadData(char* destination, size_t size)  = 0;

	operator bool() const {
		return IsStreamGood();
	}

	template <typename T, tt::enable<is_stream_deserializable_type_v<T>> = true>
	void Read(T& type) {
		if constexpr (tt::is_deserializable_v<T, StreamReader>) {
			T::Deserialize(this, type);
		} else if constexpr (tt::is_map_like_v<T>) {
			std::size_t size{ 0 };
			ReadRaw(size);
			PTGN_ASSERT(size > 0, "Failed to read raw map size");
			for (std::size_t i{ 0 }; i < size; i++) {
				typename T::key_type key;
				Read(key);
				Read(type[key]);
			}
		} else if constexpr (tt::is_std_array_v<T>) {
			std::size_t size{ 0 };
			ReadRaw(size);
			PTGN_ASSERT(size > 0, "Failed to read raw array size");
			for (std::size_t i{ 0 }; i < size; i++) {
				Read(type[i]);
			}
		} else if constexpr (tt::is_std_vector_v<T>) {
			std::size_t size{ 0 };
			ReadRaw(size);
			PTGN_ASSERT(size > 0, "Failed to read raw vector size");
			type.resize(size);
			for (std::size_t i{ 0 }; i < size; i++) {
				Read(type[i]);
			}
		} else if constexpr (std::is_same_v<T, std::string>) {
			std::size_t size{ 0 };
			ReadRaw(size);
			PTGN_ASSERT(size > 0, "Failed to read raw string size");
			type.resize(size);
			ReadBuffer(type.data(), sizeof(char) * size);
		} else if constexpr (std::is_trivially_copyable_v<T>) {
			ReadRaw(type);
		} else {
			static_assert(false, "StreamReader::is_stream_deserializable_type_v must be updated");
		}
	}

	template <typename T>
	void ReadBuffer(T* buffer, std::size_t size) {
		ReadData(reinterpret_cast<char*>(buffer), size);
	}

	template <typename T>
	void ReadRaw(T& type) {
		ReadData(reinterpret_cast<char*>(&type), sizeof(T));
	}

	template <typename... Ts>
	void ReadEntity(ecs::Entity& o) {
		static_assert(sizeof...(Ts) > 0, "Cannot deserialize entity without specified components");
		static_assert(
			(is_stream_deserializable_type_v<Ts> && ...),
			"One or more of the ReadEntity component types is not deserializable"
		);
		(Read(o.Add<Ts>()), ...);
	}
};

} // namespace ptgn