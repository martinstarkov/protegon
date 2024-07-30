#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "protegon/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/type_traits.h"

namespace ptgn {

namespace impl {

using IndexType = std::uint32_t;

} // namespace impl

class VertexArray;
class GLRenderer;

namespace type_traits {

namespace impl {

template <typename>
struct is_std_vector : std::false_type {};

template <typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template <typename>
struct is_std_array : std::false_type {};

template <typename T, std::size_t I>
struct is_std_array<std::array<T, I>> : std::true_type {};

template <typename>
struct is_std_initializer_list : std::false_type {};

template <typename T>
struct is_std_initializer_list<std::initializer_list<T>> : std::true_type {};

} // namespace impl

template <typename T>
inline constexpr bool is_std_vector_v{ impl::is_std_vector<T>::value };
template <typename T>
inline constexpr bool is_std_array_v{ impl::is_std_array<T>::value };
template <typename T>
inline constexpr bool is_std_initializer_list_v{ impl::is_std_initializer_list<T>::value };

template <typename T>
inline constexpr bool is_buffer_container_v{ is_std_vector_v<T> || is_std_array_v<T> ||
											 is_std_initializer_list_v<T> };

// Returns true if T is std::vector<ANY> or std::array<ANY, ANY>
template <typename T>
using is_buffer_container = std::enable_if_t<is_buffer_container_v<T>, bool>;

template <typename T>
inline constexpr bool is_index_buffer_container_v{
	is_buffer_container_v<T> && std::is_same_v<typename T::value_type, ptgn::impl::IndexType>
};

// Returns true if T is std::vector<IndexType> or std::array<IndexType, ANY>
template <typename T>
using is_index_buffer_container = std::enable_if_t<is_index_buffer_container_v<T>, bool>;

} // namespace type_traits

namespace impl {

struct VertexBufferInstance {
	VertexBufferInstance();
	~VertexBufferInstance();
	std::uint32_t id_{ 0 };
	impl::BufferLayout layout_;
};

struct IndexBufferInstance {
	IndexBufferInstance();
	~IndexBufferInstance();
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexBuffer : public Handle<impl::VertexBufferInstance> {
public:
	VertexBuffer()	= default;
	~VertexBuffer() = default;

	VertexBuffer(
		const void* vertex_data, std::uint32_t size, BufferUsage usage = BufferUsage::StaticDraw
	);

	void SetData(
		const void* vertex_data, std::uint32_t size, BufferUsage usage = BufferUsage::StaticDraw
	);
	void SetSubData(const void* vertex_data, std::uint32_t size);

	template <typename T, type_traits::is_buffer_container<T> = true>
	VertexBuffer(const T& vertices, BufferUsage usage = BufferUsage::StaticDraw) :
		VertexBuffer{ vertices.data(),
					  static_cast<std::uint32_t>(vertices.size() * sizeof(T::value_type)), usage } {
	}

	template <typename T, type_traits::is_buffer_container<T> = true>
	void SetData(const T& vertices, BufferUsage usage = BufferUsage::StaticDraw) {
		SetData(
			vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T::value_type)),
			usage
		);
	}

	template <typename T, type_traits::is_buffer_container<T> = true>
	void SetSubData(const T& vertices) {
		SetSubData(
			vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T::value_type))
		);
	}

	void Bind() const;
	void Unbind() const;

	template <typename... Ts, type_traits::enable<(sizeof...(Ts) > 0)> = true>
	void SetLayout() {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");
		PTGN_ASSERT(IsValid() && "Cannot set layout of uninitialized or destroyed vertex buffer");
		instance_->layout_ = CalculateLayout<Ts...>();
	}

	[[nodiscard]] const impl::BufferLayout& GetLayout() const;

private:
	friend class VertexArray;

	template <typename... Ts>
	constexpr std::array<impl::BufferElement, sizeof...(Ts)> CalculateLayout() {
		return {
			impl::BufferElement{
								static_cast<std::uint16_t>(sizeof(Ts) / std::tuple_size<Ts>::value),
								static_cast<std::uint16_t>(std::tuple_size<Ts>::value),
								impl::GetType<typename Ts::value_type>()}
			  ...
		};
	}
};

class IndexBuffer : public Handle<impl::IndexBufferInstance> {
public:
	IndexBuffer()  = default;
	~IndexBuffer() = default;

	IndexBuffer(const void* index_data, std::uint32_t size);

	void SetData(const void* index_data, std::uint32_t size);
	void SetSubData(const void* index_data, std::uint32_t size);

	template <
		typename... Ts,
		type_traits::enable<(std::is_convertible_v<Ts, impl::IndexType> && ...)> = true>
	IndexBuffer(Ts... indices) :
		IndexBuffer{ std::array<impl::IndexType, sizeof...(Ts)>{
			(static_cast<impl::IndexType>(indices), ...) } } {}

	template <typename T, type_traits::is_index_buffer_container<T> = true>
	IndexBuffer(const T& indices) :
		IndexBuffer{ indices.data(),
					 static_cast<std::uint32_t>(indices.size() * sizeof(T::value_type)) } {}

	template <typename T, type_traits::is_index_buffer_container<T> = true>
	void SetData(const T& indices) {
		SetData(indices.data(), static_cast<std::uint32_t>(indices.size() * sizeof(T::value_type)));
	}

	template <typename T, type_traits::is_index_buffer_container<T> = true>
	void SetSubData(const T& indices) {
		SetSubData(
			indices.data(), static_cast<std::uint32_t>(indices.size() * sizeof(T::value_type))
		);
	}

	void Bind() const;
	void Unbind() const;

	std::uint32_t GetCount() const;

private:
	friend class GLRenderer;

	constexpr static impl::GLType GetType() {
		return impl::GetType<impl::IndexType>();
	}

	std::uint32_t count_{ 0 };
};

} // namespace ptgn