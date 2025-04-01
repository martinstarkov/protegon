#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/entity.h"
#include "serialization/json.h"
#include "serialization/serializable.h"
#include "utility/assert.h"
#include "utility/file.h"
#include "utility/type_info.h"
#include "utility/type_traits.h"

namespace ptgn {

class JsonInputArchive {
public:
	JsonInputArchive() = delete;
	explicit JsonInputArchive(const path& filename);
	JsonInputArchive& operator=(JsonInputArchive&&) noexcept = default;
	JsonInputArchive(JsonInputArchive&&) noexcept			 = default;
	JsonInputArchive& operator=(const JsonInputArchive&)	 = delete;
	JsonInputArchive(const JsonInputArchive&)				 = delete;
	~JsonInputArchive()										 = default;

	template <typename T>
	static constexpr bool is_deserializable_v{ std::is_same_v<T, std::string> ||
											   tt::is_map_like_v<T> || tt::is_std_array_v<T> ||
											   tt::is_std_vector_v<T> ||
											   has_template_function_Deserialize_v<T> ||
											   std::is_trivially_copyable_v<T> };

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(Read(std::forward<Ts>(values)), ...);
	}

	// @return The the json object that is being written to.
	const json& GetObject() const {
		return object_ ? *object_ : data_;
	}

	json& GetObject() {
		return object_ ? *object_ : data_;
	}

	void SetObject(json* object) {
		object_ = object;
	}

	void SetObject(std::string_view key) {
		json& j{ GetObject() };
		PTGN_ASSERT(
			j.contains(key),
			"Cannot navigate into object key which does not exist in its parent json object"
		);
		object_ = &j[key];
	}

	template <typename T>
	void Read(T&& value) {
		static_assert(is_deserializable_v<T>, "Json output archive type is not deserializable");
		value_counter_++;
		Read("value" + std::to_string(value_counter_), std::forward<T>(value));
	}

	template <typename T, tt::enable<is_deserializable_v<T>> = true>
	void Read(std::string_view key, T& value) {
		if constexpr (has_template_function_Deserialize_v<T>) {
			auto* obj{ &GetObject() };
			SetObject(key);
			value.Deserialize(*this);
			SetObject(obj);
		} else {
			auto& j{ GetObject() };
			PTGN_ASSERT(j.contains(key), "Could not read key '", key, "' from json object");
			value = j[key].get<T>();
		}
	}

	template <typename... Ts>
	void ReadEntity(Entity& o) {
		static_assert(sizeof...(Ts) > 0, "Cannot deserialize entity without specified components");
		static_assert(
			(is_deserializable_v<Ts> && ...),
			"One or more of the ReadEntity component types is not deserializable"
		);
		// TODO: Fix this to read into an object.
		/*
		(Read(std::to_string(o.GetUUID()), o.Has<Ts>() ? o.Get<Ts>() : o.Add<Ts>()), ...);
		static_assert(sizeof...(Ts) > 0, "Cannot serialize entity without specified components");
		static_assert(
			(is_serializable_v<Ts> && ...),
			"One or more of the WriteEntity component types is not serializable"
		);
		const auto& uuid{ o.GetUUID() };
		auto* obj{ &GetObject() };
		std::string_view key{ std::to_string(uuid) };
		CreateObject(key);
		SetObject(key);
		(Write(type_name<Ts>(), o.Get<Ts>()), ...);
		SetObject(obj);
		*/
	}

private:
	template <typename T>
	void Read(impl::JsonKeyValuePair<T> value) {
		static_assert(is_deserializable_v<T>, "Json output archive type is not deserializable");
		Read(value.key, value.value);
	}

	// Number associated with unnamed json properties.
	std::size_t value_counter_{ 0 };
	json data_;
	// Allows for navigating into the root json for writing to child objects.
	json* object_{ nullptr };
};

class JsonOutputArchive {
public:
	JsonOutputArchive() = default;
	explicit JsonOutputArchive(const path& filename);
	JsonOutputArchive& operator=(JsonOutputArchive&&) noexcept = default;
	JsonOutputArchive(JsonOutputArchive&&) noexcept			   = default;
	JsonOutputArchive& operator=(const JsonOutputArchive&)	   = delete;
	JsonOutputArchive(const JsonOutputArchive&)				   = delete;
	~JsonOutputArchive();

	template <typename T>
	static constexpr bool is_serializable_v{ tt::is_string_like_v<T> || tt::is_map_like_v<T> ||
											 tt::is_std_array_v<T> || tt::is_std_vector_v<T> ||
											 has_template_function_Serialize_v<T> ||
											 std::is_trivially_copyable_v<T> };

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(Write(std::forward<Ts>(values)), ...);
	}

	void WriteToFile() const;

	template <typename T>
	void Write(T&& value) {
		static_assert(is_serializable_v<T>, "Json output archive type is not serializable");
		value_counter_++;
		Write("value" + std::to_string(value_counter_), std::forward<T>(value));
	}

	template <typename T, tt::enable<is_serializable_v<T>> = true>
	void Write(std::string_view key, const T& value) {
		if constexpr (has_template_function_Serialize_v<T>) {
			auto* obj{ &GetObject() };
			CreateObject(key);
			SetObject(key);
			value.Serialize(*this);
			SetObject(obj);
		} else {
			auto& j{ GetObject() };
			j[key] = value;
		}
	}

	// @return The the json object that is being written to.
	const json& GetObject() const {
		return object_ ? *object_ : data_;
	}

	json& GetObject() {
		return object_ ? *object_ : data_;
	}

	void CreateObject(std::string_view key) {
		GetObject()[key] = json::object();
	}

	void SetObject(json* object) {
		object_ = object;
	}

	void SetObject(std::string_view key) {
		json& j{ GetObject() };
		PTGN_ASSERT(
			j.contains(key),
			"Cannot navigate into object key which does not exist in its parent json object"
		);
		object_ = &j[key];
	}

	template <typename... Ts>
	void WriteEntity(const Entity& o) {
		static_assert(sizeof...(Ts) > 0, "Cannot serialize entity without specified components");
		static_assert(
			(is_serializable_v<Ts> && ...),
			"One or more of the WriteEntity component types is not serializable"
		);
		const auto& uuid{ o.GetUUID() };
		auto* obj{ &GetObject() };
		std::string_view key{ std::to_string(uuid) };
		CreateObject(key);
		SetObject(key);
		(Write(type_name<Ts>(), o.Get<Ts>()), ...);
		SetObject(obj);
	}

private:
	template <typename T>
	void Write(impl::JsonKeyValuePair<T> value) {
		static_assert(is_serializable_v<T>, "Json output archive type is not serializable");
		Write(value.key, value.value);
	}

	// Number associated with unnamed json properties.
	std::size_t value_counter_{ 0 };
	json data_;
	// Allows for navigating into the root json for writing to child objects.
	json* object_{ nullptr };
	path filepath_;
};

} // namespace ptgn