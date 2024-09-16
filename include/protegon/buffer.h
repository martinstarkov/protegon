#pragma once

#include <array>
#include <memory>
#include <vector>

#include "renderer/gl_helper.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

class VertexArray;

namespace impl {

struct BufferInstance {
	BufferInstance();
	~BufferInstance();
	std::uint32_t id_{ 0 };
};

} // namespace impl

template <BufferType BT>
class Buffer : public Handle<impl::BufferInstance> {
public:
	Buffer()		   = default;
	~Buffer() override = default;

	Buffer(const void* data, std::uint32_t size, BufferUsage usage = BufferUsage::StaticDraw) {
		Create();
		SetDataImpl(data, size, usage);
	}

	template <typename T>
	Buffer(const std::vector<T>& data, BufferUsage usage = BufferUsage::StaticDraw) :
		Buffer{ data.data(), static_cast<std::uint32_t>(data.size() * sizeof(T)), usage } {}

	template <typename T, std::size_t I>
	Buffer(const std::array<T, I>& data, BufferUsage usage = BufferUsage::StaticDraw) :
		Buffer{ data.data(), static_cast<std::uint32_t>(data.size() * sizeof(T)), usage } {
		static_assert(I > 0, "Must provide at least one buffer element");
	}

	void SetSubData(const void* data, std::uint32_t size);

	template <typename T>
	void SetSubData(const std::vector<T>& data) {
		PTGN_ASSERT(data.size() > 0, "Must provide at least one buffer element");
		SetSubData(data.data(), static_cast<std::uint32_t>(data.size() * sizeof(T)));
	}

	template <typename T, std::size_t I>
	void SetSubData(const std::array<T, I>& data) {
		static_assert(I > 0, "Must provide at least one buffer element");
		SetSubData(data.data(), static_cast<std::uint32_t>(data.size() * sizeof(T)));
	}

private:
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