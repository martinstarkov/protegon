#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <type_traits>

#include "ecs/ecs.h"
#include "serialization/serializable.h"
#include "utility/assert.h"
#include "utility/file.h"
#include "utility/type_info.h"
#include "utility/type_traits.h"

namespace ptgn {

class BinaryInputArchive {
public:
	BinaryInputArchive() = delete;
	explicit BinaryInputArchive(const path& filename);
	BinaryInputArchive& operator=(BinaryInputArchive&&) noexcept = default;
	BinaryInputArchive(BinaryInputArchive&&) noexcept			 = default;
	BinaryInputArchive& operator=(const BinaryInputArchive&)	 = delete;
	BinaryInputArchive(const BinaryInputArchive&)				 = delete;
	~BinaryInputArchive();

	template <typename T>
	static constexpr bool is_deserializable_v{ std::is_same_v<T, std::string> ||
											   tt::is_map_like_v<T> || tt::is_std_array_v<T> ||
											   tt::is_std_vector_v<T> ||
											   has_template_function_Deserialize_v<T> ||
											   std::is_trivially_copyable_v<T> };

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(ReadImpl(std::forward<Ts>(values)), ...);
	}

	bool IsStreamGood() const;

	std::uint64_t GetStreamPosition();

	void SetStreamPosition(std::uint64_t position);

	void ReadData(char* destination, size_t size);

	operator bool() const;

	template <typename T, tt::enable<is_deserializable_v<T>> = true>
	void Read(T& type) {
		if constexpr (has_template_function_Deserialize_v<T>) {
			type.Deserialize(*this);
		} else if constexpr (tt::is_map_like_v<T>) {
			std::size_t size{ 0 };
			ReadRaw(size);
			for (std::size_t i{ 0 }; i < size; i++) {
				typename T::key_type key;
				Read(key);
				Read(type[key]);
			}
		} else if constexpr (tt::is_std_array_v<T>) {
			std::size_t size{ 0 };
			ReadRaw(size);
			for (std::size_t i{ 0 }; i < size; i++) {
				Read(type[i]);
			}
		} else if constexpr (tt::is_std_vector_v<T>) {
			std::size_t size{ 0 };
			ReadRaw(size);
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
			static_assert(
				false, "BinaryInputArchive::is_stream_deserializable_type_v must be updated"
			);
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
			(is_deserializable_v<Ts> && ...),
			"One or more of the ReadEntity component types is not deserializable"
		);
		(Read(o.Has<Ts>() ? o.Get<Ts>() : o.Add<Ts>()), ...);
	}

private:
	template <typename T>
	void ReadImpl(T&& value) {
		static_assert(is_deserializable_v<T>, "Binary output archive type is not deserializable");
		Read(std::forward<T>(value));
	}

	template <typename T>
	void ReadImpl(impl::JsonKeyValuePair<T> value) {
		static_assert(is_deserializable_v<T>, "Binary output archive type is not deserializable");
		Read(value.value);
	}

	std::ifstream stream_;
};

class BinaryOutputArchive {
public:
	BinaryOutputArchive() = delete;
	explicit BinaryOutputArchive(const path& filename);
	BinaryOutputArchive& operator=(BinaryOutputArchive&&) noexcept = default;
	BinaryOutputArchive(BinaryOutputArchive&&) noexcept			   = default;
	BinaryOutputArchive& operator=(const BinaryOutputArchive&)	   = delete;
	BinaryOutputArchive(const BinaryOutputArchive&)				   = delete;
	~BinaryOutputArchive();

	template <typename T>
	static constexpr bool is_serializable_v{ tt::is_string_like_v<T> || tt::is_map_like_v<T> ||
											 tt::is_std_array_v<T> || tt::is_std_vector_v<T> ||
											 has_template_function_Serialize_v<T> ||
											 std::is_trivially_copyable_v<T> };

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(WriteImpl(std::forward<Ts>(values)), ...);
	}

	bool IsStreamGood() const;
	std::uint64_t GetStreamPosition();
	void SetStreamPosition(std::uint64_t position);
	void WriteData(const char* data, std::size_t size);

	operator bool() const;

	// Useful for inserting unknown values and then later populating them by storing their stream
	// positions.
	// @param count The number of zero bytes to write.
	void WriteZeroByte(std::size_t count);

	template <typename T, tt::enable<is_serializable_v<T>> = true>
	void Write(const T& type) {
		if constexpr (has_template_function_Serialize_v<T>) {
			type.Serialize(*this);
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
			static_assert(
				false, "BinaryOutputArchive::is_stream_serializable_type_v must be updated"
			);
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

	template <typename... Ts>
	void WriteEntity(const ecs::Entity& o) {
		static_assert(sizeof...(Ts) > 0, "Cannot serialize entity without specified components");
		static_assert(
			(is_serializable_v<Ts> && ...),
			"One or more of the WriteEntity component types is not serializable"
		);
		(Write(o.Get<Ts>()), ...);
	}

private:
	template <typename T>
	void WriteImpl(T&& value) {
		static_assert(is_serializable_v<T>, "Binary output archive type is not serializable");
		Write(std::forward<T>(value));
	}

	template <typename T>
	void WriteImpl(impl::JsonKeyValuePair<T> value) {
		static_assert(is_serializable_v<T>, "Binary output archive type is not serializable");
		Write(value.value);
	}

	std::ofstream stream_;
};

} // namespace ptgn