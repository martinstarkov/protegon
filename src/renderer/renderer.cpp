#include "renderer.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

float QuadData::GetZIndex() const {
	return vertices_[0].position[2];
}

float CircleData::GetZIndex() const {
	return vertices_[0].position[2];
}

float LineData::GetZIndex() const {
	return vertices_[0].position[2];
}

void QuadData::Add(
	const std::array<V2_float, vertex_count> vertices, float z_index, const V4_float& color,
	const std::array<V2_float, vertex_count>& tex_coords, float texture_index, float tiling_factor
) {
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].position	   = { vertices[i].x, vertices[i].y, z_index };
		vertices_[i].color		   = { color.x, color.y, color.z, color.w };
		vertices_[i].tex_coord	   = { tex_coords[i].x, tex_coords[i].y };
		vertices_[i].tex_index	   = { texture_index };
		vertices_[i].tiling_factor = { tiling_factor };
	}
}

void CircleData::Add(
	const std::array<V2_float, vertex_count> vertices, float z_index, const V4_float& color,
	float thickness, float fade
) {
	constexpr auto local = std::array<V2_float, vertex_count>{
		V2_float{ -1.0f, -1.0f },
		V2_float{ 1.0f, -1.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ -1.0f, 1.0f },
	};
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].position		= { vertices[i].x, vertices[i].y, z_index };
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

std::vector<IndexBuffer::IndexType> RendererData::GetIndices(
	const std::function<void(std::vector<IndexBuffer::IndexType>&, std::size_t, std::uint32_t)>&
		func,
	std::size_t max_indices, std::size_t vertex_count, std::size_t index_count
) {
	std::vector<IndexBuffer::IndexType> indices;
	indices.resize(max_indices);
	std::uint32_t offset{ 0 };

	for (std::size_t i{ 0 }; i < indices.size(); i += index_count) {
		func(indices, i, offset);

		offset += static_cast<std::uint32_t>(vertex_count);
	}

	return indices;
}

void RendererData::SetupBuffers() {
	IndexBuffer quad_index_buffer{ GetIndices(
		[](auto& indices, auto i, auto offset) {
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;
			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;
		},
		quad_.max_indices_, QuadData::vertex_count, QuadData::index_count
	) };

	IndexBuffer line_index_buffer{ GetIndices(
		[](auto& indices, auto i, auto offset) {
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
		},
		line_.max_indices_, LineData::vertex_count, LineData::index_count
	) };

	quad_.batch_.resize(quad_.max_vertices_);
	circle_.batch_.resize(circle_.max_vertices_);
	line_.batch_.resize(line_.max_vertices_);

	quad_.buffer_ = VertexBuffer(
		quad_.batch_,
		BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::float_>{},
		BufferUsage::DynamicDraw
	);

	circle_.buffer_ = VertexBuffer(
		circle_.batch_,
		BufferLayout<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>{},
		BufferUsage::DynamicDraw
	);

	line_.buffer_ = VertexBuffer(
		line_.batch_, BufferLayout<glsl::vec3, glsl::vec4>{}, BufferUsage::DynamicDraw
	);

	quad_.array_   = { PrimitiveMode::Triangles, quad_.buffer_, quad_index_buffer };
	circle_.array_ = { PrimitiveMode::Triangles, circle_.buffer_, quad_index_buffer };
	line_.array_   = { PrimitiveMode::Lines, line_.buffer_, line_index_buffer };
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
	for (std::uint32_t i{ 0 }; i < texture_index_; i++) {
		texture_slots_[i].Bind(i);
	}
}

float RendererData::GetTextureIndex(const Texture& texture) {
	float texture_index{ 0.0f };

	for (std::uint32_t i{ 1 }; i < texture_index_; i++) {
		if (texture_slots_[i].GetInstance() == texture.GetInstance()) {
			texture_index = (float)i;
			break;
		}
	}
	return texture_index;
}

std::array<V2_float, QuadData::vertex_count> RendererData::GetTextureCoordinates(
	const V2_float& source_position, V2_float source_size, const V2_float& texture_size
) {
	PTGN_ASSERT(!NearlyEqual(texture_size.x, 0.0f), "Texture must have width > 0");
	PTGN_ASSERT(!NearlyEqual(texture_size.y, 0.0f), "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	if (source_size.IsZero()) {
		source_size = texture_size - source_position;
	}

	V2_float src_pos{ source_position / texture_size };
	V2_float src_size{ source_size / texture_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	return { src_pos, V2_float{ src_pos.x + src_size.x, src_pos.y }, src_pos + src_size,
			 V2_float{ src_pos.x, src_pos.y + src_size.y } };
}

void RendererData::Stats::Reset() {
	quad_count	 = 0;
	circle_count = 0;
	line_count	 = 0;
	draw_calls	 = 0;
};

void RendererData::Stats::Print() {
	PTGN_INFO(
		"Draw Calls: ", draw_calls, ", Quads: ", quad_count, ", Circles: ", circle_count,
		", Lines: ", line_count
	);
}

V2_float GetDrawOffset(const V2_float& size, Origin draw_origin) {
	if (draw_origin == Origin::Center) {
		return {};
	}

	V2_float half{ size * 0.5f };
	V2_float offset;

	switch (draw_origin) {
		case Origin::TopLeft: {
			offset = -half;
			break;
		};
		case Origin::BottomRight: {
			offset = half;
			break;
		};
		case Origin::BottomLeft: {
			offset = V2_float{ -half.x, half.y };
			break;
		};
		case Origin::TopRight: {
			offset = V2_float{ half.x, -half.y };
			break;
		};
		default: PTGN_ERROR("Failed to identify draw origin");
	}

	return offset;
}

void OffsetVertices(
	std::array<V2_float, QuadData::vertex_count>& vertices, const V2_float& size, Origin draw_origin
) {
	auto draw_offset = GetDrawOffset(size, draw_origin);

	// Offset each vertex by based on draw origin.
	if (!draw_offset.IsZero()) {
		for (auto& v : vertices) {
			v -= draw_offset;
		}
	}
}

void FlipTextureCoordinates(
	std::array<V2_float, QuadData::vertex_count>& texture_coords, Flip flip
) {
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Horizontal: {
			std::swap(texture_coords[0].x, texture_coords[1].x);
			std::swap(texture_coords[2].x, texture_coords[3].x);
			break;
		}
		case Flip::Vertical: {
			std::swap(texture_coords[0].y, texture_coords[3].y);
			std::swap(texture_coords[1].y, texture_coords[2].y);
			break;
		}
		default: PTGN_ERROR("Failed to identify texture flip");
	}
}

void RotateVertices(
	std::array<V2_float, QuadData::vertex_count>& vertices, const V2_float& position,
	const V2_float& size, float rotation, const V2_float& rotation_center
) {
	PTGN_ASSERT(
		rotation_center.x >= 0.0f && rotation_center.x <= 1.0f,
		"Rotation center must be within 0.0f and 1.0f"
	);
	PTGN_ASSERT(
		rotation_center.y >= 0.0f && rotation_center.y <= 1.0f,
		"Rotation center must be within 0.0f and 1.0f"
	);

	V2_float half{ size * 0.5f };

	V2_float rot{ -size * rotation_center };

	V2_float s0{ rot };
	V2_float s1{ size.x + rot.x, rot.y };
	V2_float s2{ size + rot };
	V2_float s3{ rot.x, size.y + rot.y };

	float c{ 1.0f };
	float s{ 0.0f };

	if (!NearlyEqual(rotation, 0.0f)) {
		c = std::cos(rotation);
		s = std::sin(rotation);
	}

	auto rotated = [&](const V2_float& coordinate) -> V2_float {
		return position - rot - half +
			   V2_float{ c * coordinate.x - s * coordinate.y, s * coordinate.x + c * coordinate.y };
	};

	vertices[0] = rotated(s0);
	vertices[1] = rotated(s1);
	vertices[2] = rotated(s2);
	vertices[3] = rotated(s3);
}

std::array<V2_float, QuadData::vertex_count> GetQuadVertices(
	const V2_float& position, const V2_float& size, Origin draw_origin, float rotation,
	const V2_float& rotation_center
) {
	std::array<V2_float, QuadData::vertex_count> vertices;

	RotateVertices(vertices, position, size, rotation, rotation_center);
	OffsetVertices(vertices, size, draw_origin);

	return vertices;
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

	data_.texture_index_ = 1;
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

	auto vertices = impl::GetQuadVertices(position, size, draw_origin, rotation, rotation_center);

	PTGN_ASSERT(data_.quad_.index_ != -1);
	PTGN_ASSERT(data_.quad_.index_ < data_.quad_.batch_.size());

	data_.quad_.batch_[data_.quad_.index_].Add(
		vertices, z_index, color, texture_coords, 0.0f /* white texture */, 1.0f
	);

	data_.stats_.quad_count++;
}

void Renderer::DrawRectangleHollow(
	const V2_float& position, const V2_float& size, const Color& color, float rotation,
	const V2_float& rotation_center, float z_index, Origin draw_origin
) {
	auto vertices = impl::GetQuadVertices(position, size, draw_origin, rotation, rotation_center);

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		const V2_float& p0{ vertices[i] };
		const V2_float& p1{ vertices[(i + 1) % vertices.size()] };
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
		if (data_.texture_index_ >= data_.max_texture_slots_) {
			data_.quad_.Draw();
			data_.quad_.index_	 = 0;
			data_.texture_index_ = 1;
		}

		texture_index = (float)data_.texture_index_;

		data_.texture_slots_[data_.texture_index_] = texture;
		data_.texture_index_++;
	}

	PTGN_ASSERT(texture_index != 0.0f);

	auto texture_coords{
		impl::RendererData::GetTextureCoordinates(source_position, source_size, texture.GetSize())
	};

	impl::FlipTextureCoordinates(texture_coords, flip);

	auto vertices = impl::GetQuadVertices(
		destination_position, destination_size, draw_origin, rotation, rotation_center
	);

	PTGN_ASSERT(data_.quad_.index_ != -1);
	PTGN_ASSERT(data_.quad_.index_ < data_.quad_.batch_.size());

	data_.quad_.batch_[data_.quad_.index_].Add(
		vertices, z_index, tint_color, texture_coords, texture_index, tiling_factor
	);

	data_.stats_.quad_count++;
}

void Renderer::DrawCircleSolid(
	const V2_float& position, float radius, const Color& color, float z_index /* = 0.0f*/,
	float thickness /* = 1.0f*/, float fade /* = 0.005f*/
) {
	data_.circle_.AdvanceBatch();

	auto vertices =
		impl::GetQuadVertices(position, { radius, radius }, Origin::Center, 0.0f, { 0.5f, 0.5f });

	PTGN_ASSERT(data_.circle_.index_ != -1);
	PTGN_ASSERT(data_.circle_.index_ < data_.circle_.batch_.size());

	data_.circle_.batch_[data_.circle_.index_].Add(vertices, z_index, color, thickness, fade);

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