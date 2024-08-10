#include "renderer.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

template <>
void BatchData<QuadVertex>::AddQuad(
	const V3_float& position, const V2_float& size, const V4_float& color, Flip flip,
	const std::array<V2_float, 4>& tex_coords, float texture_index, float tiling_factor,
	Origin origin, float rotation, const V2_float& rotation_center
) {
	PTGN_ASSERT(buffer_index_ != -1);
	auto positions =
		GetQuadVertices({ position.x, position.y }, size, flip, origin, rotation, rotation_center);
	for (size_t i = 0; i < QuadVertex::VertexCount(); i++) {
		// PTGN_LOG("V", i, ": ", positions[i]);
		PTGN_ASSERT(buffer_index_ < buffer_base_.size());
		buffer_base_[buffer_index_].position	  = { positions[i].x, positions[i].y, position.z };
		buffer_base_[buffer_index_].color		  = { color.x, color.y, color.z, color.w };
		buffer_base_[buffer_index_].tex_coord	  = { tex_coords[i].x, tex_coords[i].y };
		buffer_base_[buffer_index_].tex_index	  = { texture_index };
		buffer_base_[buffer_index_].tiling_factor = { tiling_factor };
		IncrementBuffer();
	}

	index_count_ += QuadVertex::IndexCount();
}

template <>
void BatchData<CircleVertex>::AddCircle(
	const V3_float& position, const V2_float& size, const V4_float& color, float thickness,
	float fade
) {
	PTGN_ASSERT(buffer_index_ != -1);
	auto positions = GetQuadVertices(
		{ position.x, position.y }, size, Flip::None, Origin::Center, 0.0f, { 0.5f, 0.5f }
	);
	constexpr auto local = std::array<V2_float, QuadVertex::VertexCount()>{
		V2_float{ -1.0f, -1.0f },
		V2_float{ 1.0f, -1.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ -1.0f, 1.0f },
	};
	for (std::size_t i{ 0 }; i < QuadVertex::VertexCount(); i++) {
		PTGN_ASSERT(buffer_index_ < buffer_base_.size());
		buffer_base_[buffer_index_].position	   = { positions[i].x, positions[i].y, position.z };
		buffer_base_[buffer_index_].local_position = { local[i].x, local[i].y, position.z };
		buffer_base_[buffer_index_].color		   = { color.x, color.y, color.z, color.w };
		buffer_base_[buffer_index_].thickness	   = { thickness };
		buffer_base_[buffer_index_].fade		   = { fade };
		IncrementBuffer();
	}

	index_count_ += QuadVertex::IndexCount();
}

template <>
void BatchData<LineVertex>::AddLine(
	const V2_float& p0, float z_index0, const V2_float& p1, float z_index1, const V4_float& color
) {
	PTGN_ASSERT(buffer_index_ != -1);
	PTGN_ASSERT(buffer_index_ < buffer_base_.size());
	buffer_base_[buffer_index_].position = { p0.x, p0.y, z_index0 };
	buffer_base_[buffer_index_].color	 = { color.x, color.y, color.z, color.w };
	IncrementBuffer();
	PTGN_ASSERT(buffer_index_ < buffer_base_.size());
	buffer_base_[buffer_index_].position = { p1.x, p1.y, z_index1 };
	buffer_base_[buffer_index_].color	 = { color.x, color.y, color.z, color.w };
	IncrementBuffer();

	index_count_ += LineVertex::IndexCount();
}

RendererData::RendererData() {
	SetupBuffers();
	SetupTextureSlots();
	SetupShaders();
}

void RendererData::SetupBuffers() {
	IndexBuffer quad_index_buffer{ GetQuadIndices<QuadVertex, quad_.max_indices_>() };
	IndexBuffer line_index_buffer{ GetLineIndices<LineVertex, line_.max_indices_>() };

	quad_.Init<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::float_>(
		quad_.max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);

	circle_.Init<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>(
		circle_.max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);

	line_.Init<glsl::vec3, glsl::vec4>(
		line_.max_vertices_, PrimitiveMode::Lines, line_index_buffer
	);
}

void RendererData::SetupTextureSlots() {
	// First texture slot is occupied by white texture
	white_texture_ = Texture({ color::White }, { 1, 1 });

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

void RendererData::Flush() {
	if (!quad_.IsFlushed()) {
		BindTextures();
		quad_.Draw();
		stats_.draw_calls++;
	}
	if (!circle_.IsFlushed()) {
		circle_.Draw();
		stats_.draw_calls++;
	}
	if (!line_.IsFlushed()) {
		line_.Draw();
		stats_.draw_calls++;
	}
}

void RendererData::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < texture_slot_index_; i++) {
		texture_slots_[i].Bind(i);
	}
}

} // namespace impl

Renderer::Renderer() {
	GLRenderer::SetBlendMode(BlendMode::Blend);
	GLRenderer::EnableLineSmoothing();

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized . Resizing will make the viewport update during
	// resizing.
	game.event.window.Subscribe(
		WindowEvent::Resized, (void*)this,
		std::function([&](const WindowResizedEvent& e) { SetViewport(e.size); })
	);

	StartBatch();
}

Renderer::~Renderer() {
	game.event.window.Unsubscribe((void*)this);
}

void Renderer::SetBlendMode(BlendMode mode) {
	if (blend_mode_ == mode) {
		return;
	}
	blend_mode_ = mode;
	GLRenderer::SetBlendMode(mode);
}

void Renderer::SetClearColor(const Color& color) {
	if (clear_color_ == color) {
		return;
	}
	clear_color_ = color;
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

void Renderer::UpdateViewProjection(const M4_float& view_projection) {
	data_.quad_.shader_.Bind();
	data_.quad_.shader_.SetUniform("u_ViewProjection", view_projection);

	data_.circle_.shader_.Bind();
	data_.circle_.shader_.SetUniform("u_ViewProjection", view_projection);

	data_.line_.shader_.Bind();
	data_.line_.shader_.SetUniform("u_ViewProjection", view_projection);
}

void Renderer::SetViewport(const V2_int& size) {
	PTGN_ASSERT(size.x > 0 && "Cannot set viewport width below 1");
	PTGN_ASSERT(size.y > 0 && "Cannot set viewport height below 1");

	if (viewport_size_ == size) {
		return;
	}

	viewport_size_ = size;
	GLRenderer::SetViewport({}, viewport_size_);
}

void Renderer::StartBatch() {
	data_.quad_.Reset();
	data_.circle_.Reset();
	data_.line_.Reset();

	data_.texture_slot_index_ = 1;
}

void Renderer::Flush() {
	data_.Flush();
	StartBatch();
}

void Renderer::DrawRectangleFilled(
	const V2_float& position, const V2_float& size, const Color& color, float rotation /* = 0.0f*/,
	const V2_float& rotation_center, float z_index /* = 0.0f*/,
	Origin draw_origin /* = Origin::Center*/
) {
	data_.quad_.AdvanceBatch();

	data_.quad_.AddQuad(
		{ position.x, position.y, z_index }, size, color, Flip::None,
		std::array<V2_float, QuadVertex::VertexCount()>{
			V2_float{ 0.0f, 0.0f },
			V2_float{ 1.0f, 0.0f },
			V2_float{ 1.0f, 1.0f },
			V2_float{ 0.0f, 1.0f },
		},
		0.0f /* white texture */, 1.0f, draw_origin, rotation, rotation_center
	);

	data_.stats_.quad_count++;
}

void Renderer::DrawRectangleHollow(
	const V2_float& position, const V2_float& size, const Color& color, float rotation,
	const V2_float& rotation_center, float z_index, Origin origin
) {
	data_.line_.AdvanceBatch(QuadVertex::VertexCount());

	auto positions = impl::BatchData<QuadVertex>::GetQuadVertices(
		position, size, Flip::None, origin, rotation, rotation_center
	);

	PTGN_ASSERT(positions.size() == QuadVertex::VertexCount());

	data_.line_.AddLine(positions[0], z_index, positions[1], z_index, color);
	data_.line_.AddLine(positions[1], z_index, positions[2], z_index, color);
	data_.line_.AddLine(positions[2], z_index, positions[3], z_index, color);
	data_.line_.AddLine(positions[3], z_index, positions[0], z_index, color);

	data_.stats_.line_count += QuadVertex::VertexCount();
}

void Renderer::DrawTexture(
	const V2_float& destination_position, const V2_float& destination_size, const Texture& texture,
	const V2_float& source_position, V2_float source_size, float rotation,
	const V2_float& rotation_center, Flip flip, float z_index, Origin draw_origin,
	float tiling_factor, const Color& tint_color
) {
	data_.quad_.AdvanceBatch();

	V2_float texture_size{ texture.GetSize() };

	auto tex_coords{
		impl::RendererData::GetTextureCoordinates(source_position, source_size, texture_size)
	};

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
			data_.quad_.NextBatch();
			data_.texture_slot_index_ = 1;
		}

		texture_index									= (float)data_.texture_slot_index_;
		data_.texture_slots_[data_.texture_slot_index_] = texture;
		data_.texture_slot_index_++;
	}

	data_.quad_.AddQuad(
		{ destination_position.x, destination_position.y, z_index }, destination_size, tint_color,
		flip, tex_coords, texture_index, tiling_factor, draw_origin, rotation, rotation_center
	);

	data_.stats_.quad_count++;
}

void Renderer::DrawCircleSolid(
	const V2_float& position, float radius, const Color& color, float z_index /* = 0.0f*/,
	float thickness /* = 1.0f*/, float fade /* = 0.005f*/
) {
	data_.circle_.AdvanceBatch();

	data_.circle_.AddCircle(
		{ position.x, position.y, z_index }, { radius, radius }, color, thickness, fade
	);

	data_.stats_.circle_count++;
}

void Renderer::DrawLine(const V3_float& p0, const V3_float& p1, const Color& color) {
	data_.line_.AdvanceBatch();

	data_.line_.AddLine({ p0.x, p0.y }, p0.z, { p1.x, p1.y }, p1.z, color);

	data_.stats_.line_count++;
}

void Renderer::DrawLine(const V2_float& p0, const V2_float& p1, const Color& color) {
	DrawLine({ p0.x, p0.y, 0.0f }, { p1.x, p1.y, 0.0f }, color);
}

float Renderer::GetLineWidth() const {
	return data_.line_width_;
}

Color Renderer::GetClearColor() const {
	return clear_color_;
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
}

void Renderer::SetLineWidth(float width) {
	if (NearlyEqual(data_.line_width_, width)) {
		return;
	}
	data_.line_width_ = width;
	GLRenderer::SetLineWidth(data_.line_width_);
}

} // namespace ptgn