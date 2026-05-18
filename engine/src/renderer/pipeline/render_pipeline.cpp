#include "renderer/pipeline/render_pipeline.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/renderer.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn::impl {

RenderPipelineManager::RenderPipelineManager(Renderer& renderer) : renderer_{ renderer } {}

ElementBufferObject RenderPipelineManager::CreateElementBufferObject(std::uint32_t index_capacity) {
	return ElementBufferObject{ &renderer_, renderer_.gl_->buffers.CreateElementBuffer(
												nullptr, index_capacity, sizeof(Index),
												gl::BufferUsage::DynamicDraw
											) };
}

VertexBufferObject RenderPipelineManager::CreateVertexBufferObject(
	std::uint32_t vertex_capacity, std::uint32_t vertex_size
) {
	return VertexBufferObject{ &renderer_, renderer_.gl_->buffers.CreateVertexBuffer(
											   nullptr, vertex_capacity, vertex_size,
											   gl::BufferUsage::DynamicDraw
										   ) };
}

VertexArrayObject RenderPipelineManager::CreateVertexArrayObject(
	VertexBufferId vertex_buffer, const BufferLayoutView& layout, ElementBufferId element_buffer
) {
	return VertexArrayObject{ &renderer_, renderer_.gl_->vertex_arrays.CreateVertexArray(
											  vertex_buffer, layout, element_buffer
										  ) };
}

bool RenderPipelineManager::IsCurrentPipeline(std::size_t id) const {
	return id == current_pipeline_;
}

bool RenderPipelineManager::HasPipeline(std::size_t id) const {
	return std::ranges::find_if(pipelines_, [id](const auto& pair) { return pair.first == id; }) !=
		   pipelines_.end();
}

std::size_t RenderPipelineManager::GetCurrentPipelineId() const {
	return current_pipeline_;
}

void RenderPipelineManager::SetCurrentPipeline(std::size_t id) {
	current_pipeline_ = id;
}

RenderPipeline& RenderPipelineManager::GetCurrentPipeline() {
	PTGN_ASSERT(current_pipeline_ != 0, "Current pipeline must be set");
	return GetPipeline(current_pipeline_);
}

const RenderPipeline& RenderPipelineManager::GetPipeline(std::size_t id) const {
	auto it = std::ranges::find_if(pipelines_, [id](const auto& pair) { return pair.first == id; });
	PTGN_ASSERT(it != pipelines_.end(), "Pipeline not found");
	return it->second;
}

RenderPipeline& RenderPipelineManager::GetPipeline(std::size_t id) {
	auto it = std::ranges::find_if(pipelines_, [id](const auto& pair) { return pair.first == id; });
	PTGN_ASSERT(it != pipelines_.end(), "Pipeline not found");
	return it->second;
}

} // namespace ptgn::impl