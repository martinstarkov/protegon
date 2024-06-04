#pragma once

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <initializer_list>
#include <memory>

#include "protegon/shader.h"

namespace ptgn {

namespace impl {

using GLEnumType = std::uint32_t;

template <typename T> inline constexpr GLEnumType GetTypeIdentifier() {
	static_assert(false, "Could not find a type specializion for the VertexAttribPointer type");
}

template <> inline constexpr GLEnumType GetTypeIdentifier<std::int8_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::uint8_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::int16_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::uint16_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::int32_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::uint32_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::float_t>();
template <> inline constexpr GLEnumType GetTypeIdentifier<std::double_t>();

} // namespace impl

template <typename T>
class VertexBuffer;
class BufferLayout;

class BufferElement {
public:
	BufferElement(ShaderDataType data_type, bool normalized = false);
	ShaderDataType GetDataType() const;
	bool IsNormalized() const;
	std::size_t GetSize() const;
	std::size_t GetOffset() const;
private:
	BufferElement() = default;
	friend class BufferLayout;
	void SetSize(std::size_t size);
	void SetOffset(std::size_t offset);
	ShaderDataType data_type_{ ShaderDataType::none };
	std::size_t size_{ 0 };    // Number of elements x Size of element.
	std::size_t offset_{ 0 };  // Number of bytes from start of buffer.
	bool normalized_{ false }; // Whether or not the buffer elements are normalized. See here for more info: https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glVertexAttribPointer.xhtml
};

class BufferLayout {
public:
	BufferLayout() = default;
	BufferLayout(const std::initializer_list<BufferElement>& elements);
	const std::vector<BufferElement>& GetElements() const;
	std::size_t GetStride() const;
	bool IsEmpty() const;
private:
	void CalculateOffsets();
	std::vector<BufferElement> elements_;
	std::size_t stride_{ 0 };
};

class VertexArray;

class AbstractVertexBuffer {
public:
	virtual ~AbstractVertexBuffer() = default;
	using Id = std::uint32_t;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void SetLayout(const BufferLayout& layout) = 0;
	virtual const BufferLayout& GetLayout() const = 0;

	virtual Id GetId() const = 0;
protected:
	friend class VertexArray;
	virtual impl::GLEnumType GetType() = 0;
};


template <typename T = float>
class VertexBuffer : public AbstractVertexBuffer {
public:
	static std::shared_ptr<VertexBuffer<T>> Create(const std::vector<T>& vertices);

	~VertexBuffer();

	virtual void Bind() const override final;
	virtual void Unbind() const override final;

	virtual void SetLayout(const BufferLayout& layout) override final;
	virtual const BufferLayout& GetLayout() const override final;

	virtual Id GetId() const override final;

private:
	VertexBuffer() = default;
	virtual impl::GLEnumType GetType() override final {
		return impl::GetTypeIdentifier<T>();
	}

	VertexBuffer(const std::vector<T>& vertices);
	std::vector<T> vertices_;
	Id id_{ 0 };
	BufferLayout layout_;
};

class IndexBuffer {
public:
	using IndexType = std::uint32_t;
	using Id = std::uint32_t;

	static std::shared_ptr<IndexBuffer> Create(const std::vector<IndexType>& indices);

	~IndexBuffer();

	void Bind() const;
	void Unbind() const;

	std::size_t GetCount() const;
	impl::GLEnumType GetType() const;

	Id GetId() const;
private:
	IndexBuffer() = default;
	IndexBuffer(const std::vector<IndexType>& indices);
	std::vector<IndexType> indices_;
	Id id_{ 0 };
};

} // namespace ptgn