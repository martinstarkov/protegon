#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "core/util/hash.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn {

class Renderer;

namespace impl {

using PipelineId = std::size_t;

struct RenderPipeline {
	ElementBufferObject ebo;
	VertexBufferObject vbo;
	VertexArrayObject vao;

	std::uint32_t vertex_capacity{ 0 };
	std::uint32_t index_capacity{ 0 };

	PrimitiveMode primitive_mode{ PrimitiveMode::Triangles };
};

class RenderPipelineManager {
public:
	std::size_t GetCurrentPipelineId() const;

	void SetCurrentPipeline(std::size_t id);

	/// @return True if a pipeline with the given name exists.
	[[nodiscard]] bool HasPipeline(std::size_t id) const;

	/// @return True if the current pipeline matches the given name.
	[[nodiscard]] bool IsCurrentPipeline(std::size_t id) const;

	RenderPipeline& GetCurrentPipeline();

	const RenderPipeline& GetPipeline(std::size_t id) const;
	RenderPipeline& GetPipeline(std::size_t id);

	template <VertexType TVertex>
	void AddPipeline(
		std::string_view name, std::uint32_t vertex_capacity, std::uint32_t index_capacity,
		PrimitiveMode primitive_mode
	) {
		auto ebo{ CreateElementBufferObject(index_capacity) };
		auto vbo{ CreateVertexBufferObject(vertex_capacity, sizeof(typename TVertex::VertexType)) };
		auto vao{ CreateVertexArrayObject(vbo, TVertex::GetLayoutView(), ebo) };

		RenderPipeline pipeline{ .ebo			  = std::move(ebo),
								 .vbo			  = std::move(vbo),
								 .vao			  = std::move(vao),
								 .vertex_capacity = vertex_capacity,
								 .index_capacity  = index_capacity,
								 .primitive_mode  = primitive_mode };

		pipelines_.emplace_back(Hash(name), std::move(pipeline));
	}

private:
	friend class ptgn::Renderer;

	explicit RenderPipelineManager(Renderer& renderer);

	[[nodiscard]] ElementBufferObject CreateElementBufferObject(std::uint32_t index_capacity);
	[[nodiscard]] VertexBufferObject CreateVertexBufferObject(
		std::uint32_t vertex_capacity, std::uint32_t vertex_size
	);
	[[nodiscard]] VertexArrayObject CreateVertexArrayObject(
		VertexBufferId vertex_buffer, const BufferLayoutView& layout, ElementBufferId element_buffer
	);

	Renderer& renderer_;

	PipelineId current_pipeline_{ 0 };

	std::vector<std::pair<PipelineId, RenderPipeline>> pipelines_;
};

} // namespace impl

} // namespace ptgn