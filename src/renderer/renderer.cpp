#include "renderer.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

void QuadData::Add(
	const std::array<V2_float, vertex_count> positions, float z_index, const V4_float& color,
	const std::array<V2_float, vertex_count>& tex_coords, float texture_index, float tiling_factor
) {
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].position	   = { positions[i].x, positions[i].y, z_index };
		vertices_[i].color		   = { color.x, color.y, color.z, color.w };
		vertices_[i].tex_coord	   = { tex_coords[i].x, tex_coords[i].y };
		vertices_[i].tex_index	   = { texture_index };
		vertices_[i].tiling_factor = { tiling_factor };
	}
}

void CircleData::Add(
	const std::array<V2_float, vertex_count> positions, float z_index, const V4_float& color,
	float thickness, float fade
) {
	constexpr auto local = std::array<V2_float, vertex_count>{
		V2_float{ -1.0f, -1.0f },
		V2_float{ 1.0f, -1.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ -1.0f, 1.0f },
	};
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].position		= { positions[i].x, positions[i].y, z_index };
		vertices_[i].local_position = { local[i].x, local[i].y, z_index };
		vertices_[i].color			= { color.x, color.y, color.z, color.w };
		vertices_[i].thickness		= { thickness };
		vertices_[i].fade			= { fade };
	}
}

void LineData::Add(const V3_float& p0, const V3_float& p1, const V4_float& color) {
	vertices_[0].position = { p0.x, p0.y, p0.z };
	vertices_[0].color	  = { color.x, color.y, color.z, color.w };
	vertices_[1].position = { p1.x, p1.y, p1.z };
	vertices_[1].color	  = { color.x, color.y, color.z, color.w };
}

RendererData::RendererData() {
	SetupBuffers();
	SetupTextureSlots();
	SetupShaders();
}

void RendererData::SetupBuffers() {
	IndexBuffer quad_index_buffer{ GetQuadIndices<impl::QuadData, quad_.max_indices_>() };
	IndexBuffer line_index_buffer{ GetLineIndices<impl::LineData, line_.max_indices_>() };

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
	data_.quad_.index_	 = -1;
	data_.circle_.index_ = -1;
	data_.line_.index_	 = -1;

	data_.texture_slot_index_ = 1;
}

void Renderer::Flush() {
	data_.Flush();
	StartBatch();
}

void Renderer::DrawRectangleFilled(
	const V2_float& position, const V2_float& size, const Color& color, float rotation,
	const V2_float& rotation_center, float z_index, Origin draw_origin
) {
	data_.quad_.AdvanceBatch();

	std::array<V2_float, impl::QuadData::vertex_count> texture_coords{
		V2_float{ 0.0f, 0.0f },
		V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ 0.0f, 1.0f },
	};

	auto positions = impl::BatchData<impl::QuadData>::GetQuadVertices(
		{ position.x, position.y }, size, Flip::None, draw_origin, rotation, rotation_center
	);

	PTGN_ASSERT(data_.quad_.index_ != -1);
	PTGN_ASSERT(data_.quad_.index_ < data_.quad_.batch_.size());

	data_.quad_.batch_[data_.quad_.index_].Add(
		positions, z_index, color, texture_coords, 0.0f /* white texture */, 1.0f
	);

	data_.stats_.quad_count++;
}

void Renderer::DrawRectangleHollow(
	const V2_float& position, const V2_float& size, const Color& color, float rotation,
	const V2_float& rotation_center, float z_index, Origin origin
) {
	auto positions = impl::BatchData<impl::QuadData>::GetQuadVertices(
		position, size, Flip::None, origin, rotation, rotation_center
	);

	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		const V2_float& p0{ positions[i] };
		const V2_float& p1{ positions[(i + 1) % positions.size()] };
		DrawLine({ p0.x, p0.y, z_index }, { p1.x, p1.y, z_index }, color);
	}
}

void Renderer::DrawTexture(
	const V2_float& destination_position, const V2_float& destination_size, const Texture& texture,
	const V2_float& source_position, V2_float source_size, float rotation,
	const V2_float& rotation_center, Flip flip, float z_index, Origin draw_origin,
	float tiling_factor, const Color& tint_color
) {
	data_.quad_.AdvanceBatch();

	float texture_index{ data_.GetTextureIndex(texture) };

	if (texture_index == 0.0f) {
		// TODO: Optimize this if you have time. Instead of flushing the batch when the slot
		// index is beyond the slots, keep a separate texture buffer and just split that one
		// into two or more batches. This should reduce draw calls drastically.
		if (data_.texture_slot_index_ >= data_.max_texture_slots_) {
			data_.quad_.Draw();
			data_.quad_.index_		  = 0;
			data_.texture_slot_index_ = 1;
		}

		texture_index = (float)data_.texture_slot_index_;

		data_.texture_slots_[data_.texture_slot_index_] = texture;
		data_.texture_slot_index_++;
	}

	PTGN_ASSERT(texture_index != 0.0f);

	auto texture_coords{
		impl::RendererData::GetTextureCoordinates(source_position, source_size, texture.GetSize())
	};

	auto positions = impl::BatchData<impl::QuadData>::GetQuadVertices(
		{ destination_position.x, destination_position.y }, destination_size, flip, draw_origin,
		rotation, rotation_center
	);

	PTGN_ASSERT(data_.quad_.index_ != -1);
	PTGN_ASSERT(data_.quad_.index_ < data_.quad_.batch_.size());

	data_.quad_.batch_[data_.quad_.index_].Add(
		positions, z_index, tint_color, texture_coords, texture_index, tiling_factor
	);

	data_.stats_.quad_count++;
}

void Renderer::DrawCircleSolid(
	const V2_float& position, float radius, const Color& color, float z_index /* = 0.0f*/,
	float thickness /* = 1.0f*/, float fade /* = 0.005f*/
) {
	data_.circle_.AdvanceBatch();

	auto positions = impl::BatchData<impl::CircleData>::GetQuadVertices(
		{ position.x, position.y }, { radius, radius }, Flip::None, Origin::Center, 0.0f,
		{ 0.5f, 0.5f }
	);

	PTGN_ASSERT(data_.circle_.index_ != -1);
	PTGN_ASSERT(data_.circle_.index_ < data_.circle_.batch_.size());

	data_.circle_.batch_[data_.circle_.index_].Add(positions, z_index, color, thickness, fade);

	data_.stats_.circle_count++;
}

void Renderer::DrawLine(const V3_float& p0, const V3_float& p1, const Color& color) {
	data_.line_.AdvanceBatch();

	PTGN_ASSERT(data_.line_.index_ != -1);
	PTGN_ASSERT(data_.line_.index_ < data_.line_.batch_.size());

	data_.line_.batch_[data_.line_.index_].Add(p0, p1, color);

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