#include "renderer.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

template <>
void BatchData<QuadVertex>::NextBatch(RendererData& data) {
	Draw(data);
	Reset();
	data.texture_slot_index_ = 1;
}

template <>
void BatchData<CircleVertex>::NextBatch(RendererData& data) {
	Draw(data);
	Reset();
}

template <>
void BatchData<LineVertex>::NextBatch(RendererData& data) {
	Draw(data);
	Reset();
}

template <>
inline void BatchData<QuadVertex>::Draw(RendererData& data) {
	if (index_count_) {
		SetupBatch();
		data.BindTextures();
		GLRenderer::DrawElements(array_, index_count_);
		data.stats_.draw_calls++;
	}
}

template <>
inline void BatchData<CircleVertex>::Draw(RendererData& data) {
	if (index_count_) {
		SetupBatch();
		GLRenderer::DrawElements(array_, index_count_);
		data.stats_.draw_calls++;
	}
}

template <>
inline void BatchData<LineVertex>::Draw(RendererData& data) {
	if (index_count_) {
		SetupBatch();
		GLRenderer::DrawElements(array_, index_count_);
		data.stats_.draw_calls++;
	}
}

template <>
void BatchData<QuadVertex>::AddQuad(
	const V3_float& position, const V2_float& size, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, float texture_index, float tiling_factor
) {
	PTGN_ASSERT(buffer_ptr_ != nullptr);
	auto positions = GetQuadVertices(position, size);
	for (size_t i = 0; i < QuadVertex::VertexCount(); i++) {
		// PTGN_LOG("V", i, ": ", positions[i]);
		buffer_ptr_->position	   = { positions[i].x, positions[i].y, positions[i].z };
		buffer_ptr_->color		   = { color.x, color.y, color.z, color.w };
		buffer_ptr_->tex_coord	   = { tex_coords[i].x, tex_coords[i].y };
		buffer_ptr_->tex_index	   = { texture_index };
		buffer_ptr_->tiling_factor = { tiling_factor };
		IncrementBufferPtr();
	}

	index_count_ += QuadVertex::IndexCount();
}

template <>
void BatchData<CircleVertex>::AddCircle(
	const V3_float& position, const V2_float& size, const V4_float& color, float thickness,
	float fade
) {
	PTGN_ASSERT(buffer_ptr_ != nullptr);
	auto positions	   = GetQuadVertices(position, size);
	constexpr auto rel = GetRelativeVertices();
	for (std::size_t i{ 0 }; i < QuadVertex::VertexCount(); i++) {
		auto local					= rel[i] * 2.0f;
		buffer_ptr_->world_position = { positions[i].x, positions[i].y, positions[i].z };
		buffer_ptr_->local_position = { local.x, local.y, position.z };
		buffer_ptr_->color			= { color.x, color.y, color.z, color.w };
		buffer_ptr_->thickness		= { thickness };
		buffer_ptr_->fade			= { fade };
		IncrementBufferPtr();
	}

	index_count_ += QuadVertex::IndexCount();
}

template <>
void BatchData<LineVertex>::AddLine(const V3_float& p0, const V3_float& p1, const V4_float& color) {
	PTGN_ASSERT(buffer_ptr_ != nullptr);
	buffer_ptr_->position = { p0.x, p0.y, p0.z };
	buffer_ptr_->color	  = { color.x, color.y, color.z, color.w };
	IncrementBufferPtr();
	buffer_ptr_->position = { p1.x, p1.y, p1.z };
	buffer_ptr_->color	  = { color.x, color.y, color.z, color.w };
	IncrementBufferPtr();

	index_count_ += LineVertex::IndexCount();
}

RendererData::RendererData() {
	SetupBuffers();
	SetupTextureSlots();
	SetupShaders();

	// TODO: Init camera here?
}

void RendererData::SetupBuffers() {
	IndexBuffer quad_index_buffer{ GetQuadIndices<QuadVertex, max_indices_>() };
	IndexBuffer line_index_buffer{ GetLineIndices<LineVertex, max_indices_>() };

	quad_.Init<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::float_>(
		max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);

	circle_.Init<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>(
		max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);

	line_.Init<glsl::vec3, glsl::vec4>(max_vertices_, PrimitiveMode::Lines, line_index_buffer);
}

void RendererData::SetupTextureSlots() {
	// First texture slot is occupied by white texture
	std::uint32_t white_texture_data{ 0xffffffff };
	white_texture_ = Texture(&white_texture_data, { 1, 1 }, ImageFormat::RGBA8888);

	max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	texture_slots_.resize(max_texture_slots_);
	texture_slots_[0] = white_texture_;
}

void RendererData::SetupShaders() {
	PTGN_ASSERT(max_texture_slots_ > 0, "Max texture slots must be set before setting up shaders");

	std::vector<std::int32_t> samplers(max_texture_slots_);

	for (std::uint32_t i{ 0 }; i < samplers.size(); ++i) {
		samplers[i] = i;
	}

	quad_.SetupShader(
		"resources/shader/renderer_quad_vertex.glsl",
		"resources/shader/renderer_quad_fragment.glsl", samplers
	);
	circle_.SetupShader(
		"resources/shader/renderer_circle_vertex.glsl",
		"resources/shader/renderer_circle_fragment.glsl", samplers
	);
	line_.SetupShader(
		"resources/shader/renderer_line_vertex.glsl",
		"resources/shader/renderer_line_fragment.glsl", samplers
	);
}

void RendererData::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < texture_slot_index_; i++) {
		texture_slots_[i].Bind(i);
	}
}

} // namespace impl

Renderer::Renderer() {
	GLRenderer::EnableBlending();
	GLRenderer::EnableLineSmoothing();
	SetViewport(game.window.GetSize());

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized . Resizing will make the viewport update during
	// resizing.
	// TODO: Fix. This isnt working correctly. Viewport is not being set.
	game.event.window.Subscribe(
		WindowEvent::Resized, (void*)this,
		std::function([&](const WindowResizedEvent& e) { SetViewport(e.size); })
	);

	StartBatch();
}

Renderer::~Renderer() {
	// TODO: Figure out a better way to do this?
	game.event.window.Unsubscribe((void*)this);
}

void Renderer::SetClearColor(const Color& color) const {
	GLRenderer::SetClearColor(color);
}

void Renderer::Clear() const {
	GLRenderer::Clear();
}

void Renderer::Present(bool print_stats) {
	Flush();

	if (print_stats) {
		data_.stats_.Print();
	}
	data_.stats_.Reset();

	game.window.SwapBuffers();
}

void Renderer::DrawArray(const VertexArray& vertex_array) {
	PTGN_ASSERT(vertex_array.IsValid(), "Cannot submit invalid vertex array for rendering");
	GLRenderer::DrawElements(vertex_array);
	data_.stats_.draw_calls++;
}

void Renderer::SetViewport(const V2_int& size) {
	PTGN_ASSERT(size.x > 0 && "Cannot set viewport width below 1");
	PTGN_ASSERT(size.y > 0 && "Cannot set viewport height below 1");
	viewport_size_ = size;
	data_.view_projection_ =
		M4_float::Orthographic(0.0f, static_cast<float>(size.x), 0.0f, static_cast<float>(size.y));
	data_.quad_.shader_.Bind();
	data_.quad_.shader_.SetUniform("u_ViewProjection", data_.view_projection_);
	data_.circle_.shader_.Bind();
	data_.circle_.shader_.SetUniform("u_ViewProjection", data_.view_projection_);
	data_.line_.shader_.Bind();
	data_.line_.shader_.SetUniform("u_ViewProjection", data_.view_projection_);
	GLRenderer::SetViewport({}, viewport_size_);
}

void Renderer::StartBatch() {
	data_.quad_.Reset();
	data_.circle_.Reset();
	data_.line_.Reset();
	data_.texture_slot_index_ = 1;
}

std::pair<V3_float, V2_float> Renderer::GetRotated(
	const V2_float& position, const V2_float& size, float rotation, float z_index
) {
	V2_float r_pos	= position;
	V2_float r_size = size;

	if (rotation != 0.0f) {
		r_pos  = r_pos.Rotated(rotation);
		r_size = r_size.Rotated(rotation);
	}

	return {
		V3_float{r_pos.x, r_pos.y, z_index},
		   r_size
	};
}

void Renderer::Flush() {
	data_.quad_.Draw(data_);
	data_.circle_.Draw(data_);
	data_.line_.Draw(data_);
	StartBatch();
}

void Renderer::DrawRectangleFilled(
	const V2_float& position, const V2_float& size, const Color& color, float rotation /* = 0.0f*/,
	float z_index /* = 0.0f*/
) {
	if (data_.quad_.index_count_ >= data_.max_indices_) {
		data_.quad_.NextBatch(data_);
	}

	auto [p, s] = GetRotated(position, size, rotation, z_index);

	constexpr auto tex_coords{ impl::RendererData::GetTextureCoordinates() };
	constexpr const float texture_index{ 0.0f }; // white texture
	constexpr const float tiling_factor{ 1.0f };

	data_.quad_.AddQuad(p, s, color, tex_coords, texture_index, tiling_factor);
	data_.stats_.quad_count++;
}

void Renderer::DrawRectangleHollow(
	const V2_float& position, const V2_float& size, const Color& color, float rotation /* = 0.0f*/,
	float z_index /* = 0.0f*/
) {
	if (data_.line_.index_count_ >= data_.max_indices_) {
		data_.line_.NextBatch(data_);
	}

	auto [p, s] = GetRotated(position, size, rotation, z_index);

	auto positions = impl::BatchData<QuadVertex>::GetQuadVertices(p, s);

	PTGN_ASSERT(positions.size() == QuadVertex::VertexCount());

	data_.line_.AddLine(positions[0], positions[1], color);
	data_.line_.AddLine(positions[1], positions[2], color);
	data_.line_.AddLine(positions[2], positions[3], color);
	data_.line_.AddLine(positions[3], positions[0], color);

	data_.stats_.line_count += QuadVertex::VertexCount();
}

void Renderer::DrawTexture(
	const V2_float& destination_position, const V2_float& destination_size, const Texture& texture,
	const V2_float& source_position /* = {}*/, V2_float source_size /* = {}*/,
	float rotation /* = 0.0f*/, float z_index /* = 0.0f*/, float tiling_factor /* = 1.0f*/,
	const Color& tint_color /* = color::White*/
) {
	if (data_.quad_.index_count_ >= data_.max_indices_) {
		data_.quad_.NextBatch(data_);
	}

	auto [p, s] = GetRotated(destination_position, destination_size, rotation, z_index);

	V2_float tex_size{ texture.GetSize() };

	PTGN_ASSERT(!NearlyEqual(tex_size.x, 0.0f), "Texture must have width > 0");
	PTGN_ASSERT(!NearlyEqual(tex_size.y, 0.0f), "Texture must have height > 0");

	PTGN_ASSERT(source_position.x < tex_size.x, "Source position X must be within texture width");
	PTGN_ASSERT(source_position.y < tex_size.y, "Source position Y must be within texture height");

	if (source_size.IsZero()) {
		source_size = tex_size;
	}

	// TODO: Move to GPU?
	V2_float src_pos{ source_position / tex_size };
	V2_float src_size{ source_size / tex_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	auto tex_coords{ impl::RendererData::GetTextureCoordinates(src_pos, src_size) };

	float texture_index{ 0.0f };
	for (std::uint32_t i{ 1 }; i < data_.texture_slot_index_; i++) {
		if (data_.texture_slots_[i].GetInstance() == texture.GetInstance()) {
			texture_index = (float)i;
			break;
		}
	}

	if (texture_index == 0.0f) {
		// TODO: Optimize this if you have time. Instead of flushing the batch when the slot
		// index is beyond the slots, keep a separate texture buffer and just split that one
		// into two or more batches. This should reduce draw calls drastically.
		if (data_.texture_slot_index_ >= data_.max_texture_slots_) {
			data_.quad_.NextBatch(data_);
		}

		texture_index									= (float)data_.texture_slot_index_;
		data_.texture_slots_[data_.texture_slot_index_] = texture;
		data_.texture_slot_index_++;
	}

	data_.quad_.AddQuad(p, s, tint_color, tex_coords, texture_index, tiling_factor);
	data_.stats_.quad_count++;
}

void Renderer::DrawCircleSolid(
	const V2_float& position, float radius, const Color& color, float z_index /* = 0.0f*/,
	float thickness /* = 1.0f*/, float fade /* = 0.005f*/
) {
	if (data_.circle_.index_count_ >= data_.max_indices_) {
		data_.circle_.NextBatch(data_);
	}

	auto [p, s] = GetRotated(position, { radius, radius }, 0.0f, z_index);

	data_.circle_.AddCircle(p, s, color, thickness, fade);
	data_.stats_.circle_count++;
}

void Renderer::DrawLine(const V3_float& p0, const V3_float& p1, const Color& color) {
	if (data_.line_.index_count_ >= data_.max_indices_) {
		data_.line_.NextBatch(data_);
	}

	data_.line_.AddLine(p0, p1, color);
	data_.stats_.line_count++;
}

void Renderer::DrawLine(const V2_float& p0, const V2_float& p1, const Color& color) {
	DrawLine({ p0.x, p0.y, 0.0f }, { p1.x, p1.y, 0.0f }, color);
}

float Renderer::GetLineWidth() {
	return data_.line_width_;
}

void Renderer::SetLineWidth(float width) {
	if (data_.line_width_ != width) {
		data_.line_width_ = width;
		GLRenderer::SetLineWidth(data_.line_width_);
	}
}

// void Renderer::BeginScene(const OrthographicCamera& camera) {
//	data_.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
//	data_.CameraUniformBuffer.SetData(&data_.CameraBuffer, sizeof(data_.CameraData));
//
//	StartBatch();
// }
//
// void Renderer::BeginScene(const Camera& camera, const M4_float& transform) {
//	data_.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
//	data_.CameraUniformBuffer.SetData(&data_.CameraBuffer, sizeof(data_.CameraData));
//
//	StartBatch();
// }
//
// void Renderer::BeginScene(const EditorCamera& camera) {
//	data_.CameraBuffer.ViewProjection = camera.GetViewProjection();
//	data_.CameraUniformBuffer.SetData(&data_.CameraBuffer, sizeof(data_.CameraData));
//
//	StartBatch();
// }
//
// void Renderer::EndScene() {
//	Flush();
// }
//

} // namespace ptgn