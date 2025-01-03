#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "renderer/gl_types.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

class VertexArray;

namespace impl {

struct BufferInstance {
	BufferInstance() = default;
	BufferInstance(std::uint32_t count);
	~BufferInstance();
	std::uint32_t id_{ 0 };
	std::uint32_t count_{ 0 }; // max number of items in the buffer.
};

} // namespace impl

template <BufferType BT>
class Buffer : public Handle<impl::BufferInstance> {
public:
	Buffer()		   = default;
	~Buffer() override = default;

	template <typename T>
	Buffer(const T* data, std::size_t count, BufferUsage usage = BufferUsage::StaticDraw) {
		PTGN_ASSERT(count > 0, "Cannot create buffer with count 0");
		Create(static_cast<std::uint32_t>(count));
		SetDataImpl((void*)data, static_cast<std::uint32_t>(count * sizeof(T)), usage);
	}

	template <typename T>
	Buffer(
		const std::vector<T>& data, BufferUsage usage = BufferUsage::StaticDraw,
		bool use_capacity = false
	) :
		Buffer{ data.data(), use_capacity ? data.capacity() : data.size(), usage } {}

	template <typename T, std::size_t I>
	Buffer(const std::array<T, I>& data, BufferUsage usage = BufferUsage::StaticDraw) :
		Buffer{ data.data(), data.size(), usage } {
		static_assert(I > 0, "Must provide at least one buffer element");
	}

	void SetSubData(const void* data, std::uint32_t size, bool unbind_vertex_array = true);

	template <typename T>
	void SetSubData(const std::vector<T>& data, bool unbind_vertex_array = true) {
		PTGN_ASSERT(!data.empty(), "Must provide at least one buffer element");
		SetSubData(
			data.data(), static_cast<std::uint32_t>(data.size() * sizeof(T)), unbind_vertex_array
		);
	}

	template <typename T, std::size_t I>
	void SetSubData(const std::array<T, I>& data, bool unbind_vertex_array = true) {
		static_assert(I > 0, "Must provide at least one buffer element");
		SetSubData(
			data.data(), static_cast<std::uint32_t>(data.size() * sizeof(T)), unbind_vertex_array
		);
	}

	[[nodiscard]] std::uint32_t GetCount() const;

protected:
	friend class VertexArray;

	[[nodiscard]] static std::int32_t GetBoundId();
	[[nodiscard]] static std::int32_t GetBoundSize();
	[[nodiscard]] static BufferUsage GetBoundUsage();

	void Bind() const;

	void SetDataImpl(const void* data, std::uint32_t size, BufferUsage usage);
};

using VertexBuffer	= Buffer<BufferType::Vertex>;
using IndexBuffer	= Buffer<BufferType::Index>;
using UniformBuffer = Buffer<BufferType::Uniform>;

} // namespace ptgn