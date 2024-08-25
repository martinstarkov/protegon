#include "renderer.h"

#include <numeric>

#include "protegon/circle.h"
#include "protegon/game.h"
#include "protegon/line.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

float TriangulateArea(const V2_float* contour, std::size_t count) {
	PTGN_ASSERT(contour != nullptr);
	int n = static_cast<int>(count);

	float A = 0.0f;

	for (int p = n - 1, q = 0; q < n; p = q++) {
		A += contour[p].x * contour[q].y - contour[q].x * contour[p].y;
	}
	return A * 0.5f;
}

bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
) {
	float ax, ay, bx, by, cx, cy, apx, apy, bpx, bpy, cpx, cpy;
	float cCROSSap, bCROSScp, aCROSSbp;

	ax	= Cx - Bx;
	ay	= Cy - By;
	bx	= Ax - Cx;
	by	= Ay - Cy;
	cx	= Bx - Ax;
	cy	= By - Ay;
	apx = Px - Ax;
	apy = Py - Ay;
	bpx = Px - Bx;
	bpy = Py - By;
	cpx = Px - Cx;
	cpy = Py - Cy;

	aCROSSbp = ax * bpy - ay * bpx;
	cCROSSap = cx * apy - cy * apx;
	bCROSScp = bx * cpy - by * cpx;

	return ((aCROSSbp >= 0.0f) && (bCROSScp >= 0.0f) && (cCROSSap >= 0.0f));
};

bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
) {
	PTGN_ASSERT(contour != nullptr);
	int p;
	float Ax, Ay, Bx, By, Cx, Cy, Px, Py;

	Ax = contour[V[u]].x;
	Ay = contour[V[u]].y;

	Bx = contour[V[v]].x;
	By = contour[V[v]].y;

	Cx = contour[V[w]].x;
	Cy = contour[V[w]].y;

	float res = (Bx - Ax) * (Cy - Ay) - (By - Ay) * (Cx - Ax);

	if (NearlyEqual(res, 0.0f)) {
		return false;
	}

	for (p = 0; p < n; p++) {
		if ((p == u) || (p == v) || (p == w)) {
			continue;
		}
		Px = contour[V[p]].x;
		Py = contour[V[p]].y;
		if (TriangulateInsideTriangle(Ax, Ay, Bx, By, Cx, Cy, Px, Py)) {
			return false;
		}
	}

	return true;
}

// From: https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml
std::vector<Triangle<float>> TriangulateProcess(const V2_float* contour, std::size_t count) {
	PTGN_ASSERT(contour != nullptr);
	/* allocate and initialize list of Vertices in polygon */
	std::vector<Triangle<float>> result;

	int n = static_cast<int>(count);
	if (n < 3) {
		return result;
	}

	std::vector<int> V(n);

	/* we want a counter-clockwise polygon in V */

	if (0.0f < TriangulateArea(contour, count)) {
		for (int v = 0; v < n; v++) {
			V[v] = v;
		}
	} else {
		for (int v = 0; v < n; v++) {
			V[v] = (n - 1) - v;
		}
	}

	int nv = n;

	/*  remove nv-2 Vertices, creating 1 triangle every time */
	int r_count = 2 * nv; /* error detection */

	for (int m = 0, v = nv - 1; nv > 2;) {
		/* if we loop, it is probably a non-simple polygon */
		if (0 >= (r_count--)) {
			//** Triangulate: ERROR - probable bad polygon!
			return result;
		}

		/* three consecutive vertices in current polygon, <u,v,w> */
		int u = v;
		if (nv <= u) {
			u = 0; /* previous */
		}
		v = u + 1;
		if (nv <= v) {
			v = 0; /* new v    */
		}
		int w = v + 1;
		if (nv <= w) {
			w = 0; /* next     */
		}

		if (TriangulateSnip(contour, u, v, w, nv, V)) {
			int a, b, c, s, t;

			/* true names of the vertices */
			a = V[u];
			b = V[v];
			c = V[w];

			/* output Triangle */
			result.push_back({ contour[a], contour[b], contour[c] });

			m++;

			/* remove v from remaining polygon */
			for (s = v, t = v + 1; t < nv; s++, t++) {
				V[s] = V[t];
			}
			nv--;

			/* resest error detection counter */
			r_count = 2 * nv;
		}
	}

	return result;
}

void QuadData::Add(
	const std::array<V2_float, vertex_count>& vertices, float z_index, const V4_float& color,
	const std::array<V2_float, vertex_count>& tex_coords, float texture_index
) {
	ShapeData::Add(vertices, z_index, color);
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
		vertices_[i].tex_index = { texture_index };
	}
}

void CircleData::Add(
	const std::array<V2_float, vertex_count>& vertices, float z_index, const V4_float& color,
	float line_width, float fade
) {
	ShapeData::Add(vertices, z_index, color);
	constexpr auto local = std::array<V2_float, vertex_count>{
		V2_float{ -1.0f, -1.0f },
		V2_float{ 1.0f, -1.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ -1.0f, 1.0f },
	};
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].local_position = { local[i].x, local[i].y, z_index };
		vertices_[i].line_width		= { line_width };
		vertices_[i].fade			= { fade };
	}
}

template <typename T>
void BatchData<T>::Draw() {
	PTGN_ASSERT(index_ != -1);
	// TODO: Fix sorting.
	// Sort by z-index before sending to GPU.
	/*std::sort(batch_.begin(), batch_.begin() + index_ + 1, [](const T& a, const T& b) {
		return a.GetZIndex() < b.GetZIndex();
	});*/
	array_.GetVertexBuffer().SetSubData(
		batch_.data(), static_cast<std::uint32_t>(index_ + 1) * sizeof(T)
	);
	shader_.Bind();
	GLRenderer::DrawElements(array_, (index_ + 1) * T::index_count);
	index_ = -1;
}

RendererData::RendererData() {
	SetupBuffers();
	SetupTextureSlots();
	SetupShaders();
}

void RendererData::SetupBuffers() {
	auto get_indices = [](std::size_t max_indices, const auto& generator) {
		std::vector<std::uint32_t> indices;
		indices.resize(max_indices);
		std::generate(indices.begin(), indices.end(), generator);
		return indices;
	};

	constexpr std::array<std::uint32_t, QuadData::index_count> quad_index_pattern{
		0, 1, 2, 2, 3, 0
	};

	auto quad_generator = [&, offset = 0, pattern_index = 0]() mutable {
		auto index = offset + quad_index_pattern[pattern_index];
		pattern_index++;
		if (pattern_index % QuadData::index_count == 0) {
			offset		  += QuadData::vertex_count;
			pattern_index  = 0;
		}
		return index;
	};

	auto iota = [i = 0]() mutable {
		return i++;
	};

	IndexBuffer quad_ib{ get_indices(BatchData<QuadData>::max_indices_, quad_generator) };
	IndexBuffer triangle_ib{ get_indices(BatchData<TriangleData>::max_indices_, iota) };
	IndexBuffer line_ib{ get_indices(BatchData<LineData>::max_indices_, iota) };
	IndexBuffer point_ib{ get_indices(BatchData<PointData>::max_indices_, iota) };

	quad_.batch_.resize(quad_.max_vertices_);
	circle_.batch_.resize(circle_.max_vertices_);
	triangle_.batch_.resize(triangle_.max_vertices_);
	line_.batch_.resize(line_.max_vertices_);
	point_.batch_.resize(point_.max_vertices_);

	auto set_array = [&](auto& data, PrimitiveMode p, const impl::InternalBufferLayout& layout,
						 const IndexBuffer& ib) {
		data.array_ = { p, VertexBuffer(data.batch_, BufferUsage::DynamicDraw), layout, ib };
	};

	auto quad_vert{ BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>{} };

	auto circle_vert{
		BufferLayout<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>{}
	};

	auto color_vert{ BufferLayout<glsl::vec3, glsl::vec4>{} };

	set_array(quad_, PrimitiveMode::Triangles, quad_vert, quad_ib);
	set_array(circle_, PrimitiveMode::Triangles, circle_vert, quad_ib);
	set_array(triangle_, PrimitiveMode::Triangles, color_vert, triangle_ib);
	set_array(line_, PrimitiveMode::Lines, color_vert, line_ib);
	set_array(point_, PrimitiveMode::Points, color_vert, point_ib);
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

	PTGN_INFO("Renderer Texture Slots: ", max_texture_slots_);
	// This strange way of including files allows for them to be packed into the library binary.
	ShaderSource quad_frag;

	if (max_texture_slots_ == 8) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_8.frag)
		};
	} else if (max_texture_slots_ == 16) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_16.frag)
		};
	} else if (max_texture_slots_ == 32) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_32.frag)
		};
	} else {
		PTGN_ERROR("Unsupported Texture Slot Size: ", max_texture_slots_);
	}

	quad_.shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(quad.vert)
		},
		quad_frag
	);

	circle_.shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(circle.vert)
		},
		ShaderSource{
#include PTGN_SHADER_PATH(circle.frag)
		}
	);

	point_.shader_ = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
		},
		ShaderSource{
#include PTGN_SHADER_PATH(color.frag)
		}
	);

	line_.shader_	  = point_.shader_;
	triangle_.shader_ = point_.shader_;

	std::vector<std::int32_t> samplers(max_texture_slots_);

	for (std::uint32_t i{ 0 }; i < samplers.size(); ++i) {
		samplers[i] = i;
	}

	quad_.shader_.Bind();
	quad_.shader_.SetUniform("u_Textures", samplers.data(), samplers.size());
}

template <typename T>
void BatchData<T>::Flush(RendererData& data) {
	if (!IsFlushed()) {
		Draw();
		data.draw_calls++;
	}
}

template <>
void BatchData<QuadData>::Flush(RendererData& data) {
	if (!IsFlushed()) {
		data.BindTextures();
		Draw();
		data.draw_calls++;
	}
}

void RendererData::Flush() {
	quad_.Flush(*this);
	circle_.Flush(*this);
	triangle_.Flush(*this);
	line_.Flush(*this);
	point_.Flush(*this);
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
	if (texture_index == 0.0f) {
		// TODO: Add new batch once texture slots fill up on the current batch.

		// TODO: Optimize this if you have time. Instead of flushing the batch when the slot
		// index is beyond the slots, keep a separate texture buffer and just split that one
		// into two or more batches. This should reduce draw calls drastically.
		if (texture_index_ >= max_texture_slots_) {
			quad_.Draw();
			draw_calls++;
			quad_.index_   = 0;
			texture_index_ = 1;
		}

		texture_index = (float)texture_index_;

		texture_slots_[texture_index_] = texture;
		texture_index_++;
	}

	PTGN_ASSERT(texture_index != 0.0f);

	return texture_index;
}

std::array<V2_float, QuadData::vertex_count> RendererData::GetTextureCoordinates(
	const V2_float& source_position, V2_float source_size, const V2_float& texture_size, Flip flip
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

	// Convert to 0 -> 1 range.
	V2_float src_pos{ source_position / texture_size };
	V2_float src_size{ source_size / texture_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	V2_float half_pixel{ 0.5f / texture_size };

	std::array<V2_float, QuadData::vertex_count> texture_coordinates{
		src_pos + half_pixel,
		V2_float{ src_pos.x + src_size.x - half_pixel.x, src_pos.y + half_pixel.y },
		src_pos + src_size - half_pixel,
		V2_float{ src_pos.x + half_pixel.x, src_pos.y + src_size.y - half_pixel.y },
	};

	FlipTextureCoordinates(texture_coordinates, flip);

	return texture_coordinates;
}

void OffsetVertices(
	std::array<V2_float, QuadData::vertex_count>& vertices, const V2_float& size, Origin draw_origin
) {
	auto draw_offset = GetOffsetFromCenter(size, draw_origin);

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

template class BatchData<QuadData>;
template class BatchData<CircleData>;
template class BatchData<LineData>;
template class BatchData<TriangleData>;
template class BatchData<PointData>;

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

Color Renderer::GetClearColor() const {
	return clear_color_;
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
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

void Renderer::Present() {
	std::int32_t quad_count{ data_.quad_.index_ + 1 };
	std::int32_t circle_count{ data_.circle_.index_ + 1 };
	std::int32_t triangle_count{ data_.triangle_.index_ + 1 };
	std::int32_t line_count{ data_.line_.index_ + 1 };
	std::int32_t point_count{ data_.point_.index_ + 1 };

	Flush();

	PTGN_INFO(
		"Draw Calls: ", data_.draw_calls, ", Quads: ", quad_count, ", Triangles: ", triangle_count,
		", Circles: ", circle_count, ", Lines: ", line_count, ", Points: ", point_count
	);
	data_.draw_calls = 0;

	game.window.SwapBuffers();
}

void Renderer::UpdateViewProjection(const M4_float& view_projection) {
	data_.quad_.shader_.Bind();
	data_.quad_.shader_.SetUniform("u_ViewProjection", view_projection);

	data_.circle_.shader_.Bind();
	data_.circle_.shader_.SetUniform("u_ViewProjection", view_projection);

	data_.point_.shader_.Bind();
	data_.point_.shader_.SetUniform("u_ViewProjection", view_projection);

	// triangle_ and line_ share the shader point_ so no need to set them.
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
	data_.quad_.index_	   = -1;
	data_.circle_.index_   = -1;
	data_.line_.index_	   = -1;
	data_.triangle_.index_ = -1;
	data_.point_.index_	   = -1;

	data_.texture_index_ = 1;
}

void Renderer::Flush() {
	data_.Flush();
	StartBatch();
}

void Renderer::DrawElements(const VertexArray& va, std::size_t index_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot submit invalid vertex array for rendering");
	PTGN_ASSERT(
		va.HasVertexBuffer(), "Cannot submit vertex array without a set vertex buffer for rendering"
	);
	PTGN_ASSERT(
		va.HasIndexBuffer(), "Cannot submit vertex array without a set index buffer for rendering"
	);
	GLRenderer::DrawElements(va, index_count);
	data_.draw_calls++;
}

void Renderer::DrawArrays(const VertexArray& va, std::size_t vertex_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot submit invalid vertex array for rendering");
	PTGN_ASSERT(
		va.HasVertexBuffer(), "Cannot submit vertex array without a set vertex buffer for rendering"
	);
	GLRenderer::DrawArrays(va, vertex_count);
	data_.draw_calls++;
}

void Renderer::DrawTextureImpl(
	const std::array<V2_float, 4>& vertices, float texture_index,
	const std::array<V2_float, 4>& tex_coords, const V4_float& tint_color, float z
) {
	data_.quad_.Get().Add(vertices, z, tint_color, tex_coords, texture_index);
}

void Renderer::DrawEllipseHollowImpl(
	const V2_float& p, const V2_float& r, const V4_float& col, float lw, float fade, float z
) {
	PTGN_ASSERT(lw >= 0.0f, "Cannot draw negative line width");

	auto vertices =
		impl::GetQuadVertices(p, { r.x * 2.0f, r.y * 2.0f }, Origin::Center, 0.0f, { 0.5f, 0.5f });

	// Internally line width for a filled rectangle is 1.0f and a completely hollow one is 0.0f, but
	// in the API the line width is expected in pixels.
	// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected bugs.
	lw = NearlyEqual(lw, 0.0f) ? 1.0f : fade + lw / std::min(r.x, r.y);

	data_.circle_.Get().Add(vertices, z, col, lw, fade);
}

void Renderer::DrawTriangleFilledImpl(
	const V2_float& a, const V2_float& b, const V2_float& c, const V4_float& col, float z
) {
	data_.triangle_.Get().Add({ a, b, c }, z, col);
}

void Renderer::DrawLineImpl(
	const V2_float& p0, const V2_float& p1, const V4_float& col, float lw, float z
) {
	PTGN_ASSERT(lw >= 0.0f, "Cannot draw negative line width");

	if (lw > 1.0f) {
		V2_float d{ p1 - p0 };
		// TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
		auto vertices = impl::GetQuadVertices(
			p0 + d * 0.5f, { d.Magnitude(), lw }, Origin::Center, d.Angle(), { 0.5f, 0.5f }
		);
		DrawRectangleFilledImpl(vertices, col, z);
		return;
	}

	data_.line_.Get().Add({ p0, p1 }, z, col);
}

void Renderer::DrawPointImpl(const V2_float& p, const V4_float& col, float r, float z) {
	if (r <= 1.0f) {
		data_.point_.Get().Add({ p }, z, col);
	} else {
		DrawEllipseFilledImpl(p, { r, r }, col, 0.005f, z);
	}
}

void Renderer::DrawRectangleFilledImpl(
	const std::array<V2_float, 4>& vertices, const V4_float& col, float z
) {
	DrawTextureImpl(
		vertices, 0.0f,
		{
			V2_float{ 0.0f, 0.0f },
			V2_float{ 1.0f, 0.0f },
			V2_float{ 1.0f, 1.0f },
			V2_float{ 0.0f, 1.0f },
		},
		col, z
	);
}

void Renderer::DrawTriangleHollowImpl(
	const V2_float& a, const V2_float& b, const V2_float& c, const V4_float& col, float lw, float z
) {
	std::array<V2_float, impl::TriangleData::vertex_count> vertices{ a, b, c };
	DrawPolygonHollowImpl(vertices.data(), vertices.size(), col, lw, z);
}

void Renderer::DrawRectangleHollowImpl(
	const std::array<V2_float, 4>& vertices, const V4_float& col, float lw, float z
) {
	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		DrawLineImpl(vertices[i], vertices[(i + 1) % vertices.size()], col, lw, z);
	}
}

void Renderer::DrawRoundedRectangleFilledImpl(
	const V2_float& p, const V2_float& s, float rad, const V4_float& col, Origin o, float rot,
	const V2_float& rc, float z
) {
	PTGN_ASSERT(
		2.0f * rad < s.x, "Cannot draw rounded rectangle with larger radius than half its width"
	);
	PTGN_ASSERT(
		2.0f * rad < s.y, "Cannot draw rounded rectangle with larger radius than half its height"
	);

	V2_float offset = GetOffsetFromCenter(s, o);

	auto inner_vertices =
		impl::GetQuadVertices(p - offset, s - V2_float{ rad * 2 }, Origin::Center, rot, rc);

	DrawRectangleFilledImpl(inner_vertices, col, z);

	DrawArcFilledImpl(inner_vertices[0], rad, rot - 180.0f, rot - 90.0f, col, z);
	DrawArcFilledImpl(inner_vertices[1], rad, rot - 90.0f, rot + 0.0f, col, z);
	DrawArcFilledImpl(inner_vertices[2], rad, rot + 0.0f, rot + 90.0f, col, z);
	DrawArcFilledImpl(inner_vertices[3], rad, rot + 90.0f, rot + 180.0f, col, z);

	V2_float t = V2_float(rad / 2.0f, 0.0f).Rotated(DegToRad(rot - 90.0f));
	V2_float r = V2_float(rad / 2.0f, 0.0f).Rotated(DegToRad(rot + 0.0f));
	V2_float b = V2_float(rad / 2.0f, 0.0f).Rotated(DegToRad(rot + 90.0f));
	V2_float l = V2_float(rad / 2.0f, 0.0f).Rotated(DegToRad(rot - 180.0f));

	DrawLineImpl(inner_vertices[0] + t, inner_vertices[1] + t, col, rad, z);
	DrawLineImpl(inner_vertices[1] + r, inner_vertices[2] + r, col, rad, z);
	DrawLineImpl(inner_vertices[2] + b, inner_vertices[3] + b, col, rad, z);
	DrawLineImpl(inner_vertices[3] + l, inner_vertices[0] + l, col, rad, z);
}

void Renderer::DrawRoundedRectangleHollowImpl(
	const V2_float& p, const V2_float& s, float rad, const V4_float& col, Origin o, float lw,
	float rot, const V2_float& rc, float z
) {
	PTGN_ASSERT(
		2.0f * rad < s.x, "Cannot draw rounded rectangle with larger radius than half its width"
	);
	PTGN_ASSERT(
		2.0f * rad < s.y, "Cannot draw rounded rectangle with larger radius than half its height"
	);

	V2_float offset = GetOffsetFromCenter(s, o);

	auto inner_vertices =
		impl::GetQuadVertices(p - offset, s - V2_float{ rad * 2 }, Origin::Center, rot, rc);

	DrawArcHollowImpl(inner_vertices[0], rad, rot - 180.0f, rot - 90.0f, col, lw, z);
	DrawArcHollowImpl(inner_vertices[1], rad, rot - 90.0f, rot + 0.0f, col, lw, z);
	DrawArcHollowImpl(inner_vertices[2], rad, rot + 0.0f, rot + 90.0f, col, lw, z);
	DrawArcHollowImpl(inner_vertices[3], rad, rot + 90.0f, rot + 180.0f, col, lw, z);

	V2_float t = V2_float(rad, 0.0f).Rotated(DegToRad(rot - 90.0f));
	V2_float r = V2_float(rad, 0.0f).Rotated(DegToRad(rot + 0.0f));
	V2_float b = V2_float(rad, 0.0f).Rotated(DegToRad(rot + 90.0f));
	V2_float l = V2_float(rad, 0.0f).Rotated(DegToRad(rot - 180.0f));

	DrawLineImpl(inner_vertices[0] + t, inner_vertices[1] + t, col, lw, z);
	DrawLineImpl(inner_vertices[1] + r, inner_vertices[2] + r, col, lw, z);
	DrawLineImpl(inner_vertices[2] + b, inner_vertices[3] + b, col, lw, z);
	DrawLineImpl(inner_vertices[3] + l, inner_vertices[0] + l, col, lw, z);
}

void Renderer::DrawEllipseFilledImpl(
	const V2_float& p, const V2_float& r, const V4_float& col, float fade, float z
) {
	DrawEllipseHollowImpl(p, r, col, 0.0f, fade, z);
}

void Renderer::DrawArcImpl(
	const V2_float& p, float arc_radius, float start_angle, float end_angle, const V4_float& col,
	float lw, float z, bool filled
) {
	PTGN_ASSERT(arc_radius >= 0.0f, "Cannot draw filled arc with negative radius");

	start_angle = ClampAngle360(start_angle);
	end_angle	= ClampAngle360(end_angle);

	if (NearlyEqual(arc_radius, 0.0f)) {
		DrawPointImpl(p, col, 1.0f, z);
		return;
	}

	// float delta_angle = two_pi<float> / arc_radius;

	start_angle = DegToRad(start_angle);
	end_angle	= DegToRad(end_angle);

	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	float arc = end_angle - start_angle;

	// Number of triangles the arc is divided into.
	std::size_t resolution =
		std::max(static_cast<std::size_t>(360), static_cast<std::size_t>(30.0f * arc_radius));

	std::size_t n{ resolution };

	float delta_angle{ arc / static_cast<float>(n) };

	PTGN_ASSERT(arc >= 0.0f);

	if (n > 1) {
		std::vector<V2_float> v(n);

		for (std::size_t i{ 0 }; i < n; i++) {
			float angle = start_angle + i * delta_angle;
			v[i] = { p.x + arc_radius * std::cos(angle), p.y + arc_radius * std::sin(angle) };
		}

		if (filled) {
			for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
				DrawTriangleFilledImpl(p, v[i], v[i + 1], col, z);
			}
		} else {
			PTGN_ASSERT(lw >= 0.0f, "Must provide valid line width when drawing hollow arc");
			for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
				DrawLineImpl(v[i], v[i + 1], col, lw, z);
			}
		}
	} else {
		DrawPointImpl(p, col, 1.0f, z);
	}
}

void Renderer::DrawArcFilledImpl(
	const V2_float& p, float arc_radius, float start_angle, float end_angle, const V4_float& col,
	float z
) {
	DrawArcImpl(p, arc_radius, start_angle, end_angle, col, 0.0f, z, true);
}

void Renderer::DrawArcHollowImpl(
	const V2_float& p, float arc_radius, float start_angle, float end_angle, const V4_float& col,
	float lw, float z
) {
	DrawArcImpl(p, arc_radius, start_angle, end_angle, col, lw, z, false);
}

void Renderer::DrawCapsuleFilledImpl(
	const V2_float& p0, const V2_float& p1, float r, const V4_float& col, float fade, float z
) {
	V2_float dir{ p1 - p0 };
	const float angle{ RadToDeg(ClampAngle2Pi(dir.Angle() + half_pi<float>)) };
	const float dir2{ dir.Dot(dir) };

	V2_float tangent_r;

	// Note that dir2 is an int.
	if (NearlyEqual(dir2, 0.0f)) {
		DrawEllipseFilledImpl(p0, { r, r }, col, fade, z);
		return;
	} else {
		V2_float tmp = dir.Skewed() / std::sqrt(dir2) * r;
		tangent_r	 = { FastFloor(tmp.x), FastFloor(tmp.y) };
	}

	// Draw central line.
	DrawLineImpl(p0, p1, col, r * 2.0f, z);

	// Draw edge arcs.
	float delta{ 0.5f };
	DrawArcFilledImpl(p0, r, angle - delta, angle + 180.0f + delta, col, z);
	DrawArcFilledImpl(p1, r, angle + 180.0f - delta, angle + delta, col, z);
}

void Renderer::DrawCapsuleHollowImpl(
	const V2_float& p0, const V2_float& p1, float r, const V4_float& col, float lw, float fade,
	float z
) {
	V2_float dir{ p1 - p0 };
	const float angle{ RadToDeg(ClampAngle2Pi(dir.Angle() + half_pi<float>)) };
	const float dir2{ dir.Dot(dir) };

	V2_float tangent_r;

	// Note that dir2 is an int.
	if (NearlyEqual(dir2, 0.0f)) {
		DrawEllipseHollowImpl(p0, { r, r }, col, lw, fade, z);
		return;
	} else {
		V2_float tmp = dir.Skewed() / std::sqrt(dir2) * r;
		tangent_r	 = { FastFloor(tmp.x), FastFloor(tmp.y) };
	}

	// Draw edge lines.
	DrawLineImpl(p0 + tangent_r, p1 + tangent_r, col, lw, z);
	DrawLineImpl(p0 - tangent_r, p1 - tangent_r, col, lw, z);

	// Draw edge arcs.
	DrawArcHollowImpl(p0, r, angle, angle + 180.0f, col, lw, z);
	DrawArcHollowImpl(p1, r, angle + 180.0f, angle, col, lw, z);
}

void Renderer::DrawPolygonFilledImpl(
	const Triangle<float>* triangles, std::size_t triangle_count, const V4_float& col, float z
) {
	PTGN_ASSERT(triangles != nullptr);

	for (std::size_t i{ 0 }; i < triangle_count; ++i) {
		const Triangle<float>& t{ triangles[i] };
		DrawTriangleFilledImpl(t.a, t.b, t.c, col, z);
	}
}

void Renderer::DrawPolygonHollowImpl(
	const V2_float* vertices, std::size_t vertex_count, const V4_float& col, float lw, float z
) {
	PTGN_ASSERT(vertices != nullptr);
	for (std::size_t i{ 0 }; i < vertex_count; i++) {
		DrawLineImpl(vertices[i], vertices[(i + 1) % vertex_count], col, lw, z);
	}
}

void Renderer::DrawTexture(
	const Texture& texture, const V2_float& destination_position, const V2_float& destination_size,
	const V2_float& source_position, V2_float source_size, Origin draw_origin, Flip flip,
	float rotation, const V2_float& rotation_center, float z_index, const Color& tint_color
) {
	float texture_index{ data_.GetTextureIndex(texture) };

	auto tex_coords{ impl::RendererData::GetTextureCoordinates(
		source_position, source_size, texture.GetSize(), flip
	) };

	auto vertices = impl::GetQuadVertices(
		destination_position, destination_size, draw_origin, rotation, rotation_center
	);

	DrawTextureImpl(vertices, texture_index, tex_coords, tint_color.Normalized(), z_index);
}

void Renderer::DrawPoint(
	const V2_float& position, const Color& color, float radius, float z_index
) {
	DrawPointImpl(position, color.Normalized(), radius, z_index);
}

void Renderer::DrawLine(
	const V2_float& p0, const V2_float& p1, const Color& color, float line_width, float z_index
) {
	DrawLineImpl(p0, p1, color.Normalized(), line_width, z_index);
}

void Renderer::DrawLine(
	const Line<float>& line, const Color& color, float line_width, float z_index
) {
	DrawLineImpl(line.a, line.b, color.Normalized(), line_width, z_index);
}

void Renderer::DrawTriangleFilled(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float z_index
) {
	DrawTriangleFilledImpl(a, b, c, color.Normalized(), z_index);
}

void Renderer::DrawTriangleHollow(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float line_width,
	float z_index
) {
	DrawTriangleHollowImpl(a, b, c, color.Normalized(), line_width, z_index);
}

void Renderer::DrawTriangleFilled(
	const Triangle<float>& triangle, const Color& color, float z_index
) {
	DrawTriangleFilledImpl(triangle.a, triangle.b, triangle.c, color.Normalized(), z_index);
}

void Renderer::DrawTriangleHollow(
	const Triangle<float>& triangle, const Color& color, float line_width, float z_index
) {
	DrawTriangleHollowImpl(
		triangle.a, triangle.b, triangle.c, color.Normalized(), line_width, z_index
	);
}

// Rotation in degrees.
void Renderer::DrawRectangleFilled(
	const V2_float& position, const V2_float& size, const Color& color, Origin draw_origin,
	float rotation, const V2_float& rotation_center, float z_index
) {
	auto vertices = impl::GetQuadVertices(position, size, draw_origin, rotation, rotation_center);
	DrawRectangleFilledImpl(vertices, color.Normalized(), z_index);
}

// Rotation in degrees.
void Renderer::DrawRectangleFilled(
	const Rectangle<float>& rectangle, const Color& color, float rotation,
	const V2_float& rotation_center, float z_index
) {
	auto vertices = impl::GetQuadVertices(
		rectangle.pos, rectangle.size, rectangle.origin, rotation, rotation_center
	);
	DrawRectangleFilledImpl(vertices, color.Normalized(), z_index);
}

// Rotation in degrees.
void Renderer::DrawRectangleHollow(
	const V2_float& position, const V2_float& size, const Color& color, Origin draw_origin,
	float line_width, float rotation, const V2_float& rotation_center, float z_index
) {
	auto vertices{ impl::GetQuadVertices(position, size, draw_origin, rotation, rotation_center) };
	DrawRectangleHollowImpl(vertices, color.Normalized(), line_width, z_index);
}

// Rotation in degrees.
void Renderer::DrawRectangleHollow(
	const Rectangle<float>& rectangle, const Color& color, float line_width, float rotation,
	const V2_float& rotation_center, float z_index
) {
	auto vertices{ impl::GetQuadVertices(
		rectangle.pos, rectangle.size, rectangle.origin, rotation, rotation_center
	) };
	DrawRectangleHollowImpl(vertices, color.Normalized(), line_width, z_index);
}

void Renderer::DrawPolygonFilled(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float z_index
) {
	PTGN_ASSERT(vertices != nullptr);
	auto triangles = impl::TriangulateProcess(vertices, vertex_count);
	DrawPolygonFilledImpl(triangles.data(), triangles.size(), color.Normalized(), z_index);
}

void Renderer::DrawPolygonFilled(const Polygon& polygon, const Color& color, float z_index) {
	auto triangles = impl::TriangulateProcess(polygon.vertices.data(), polygon.vertices.size());
	DrawPolygonFilledImpl(triangles.data(), triangles.size(), color.Normalized(), z_index);
}

void Renderer::DrawPolygonHollow(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float line_width,
	float z_index
) {
	PTGN_ASSERT(vertices != nullptr);
	DrawPolygonHollowImpl(vertices, vertex_count, color.Normalized(), line_width, z_index);
}

void Renderer::DrawPolygonHollow(
	const Polygon& polygon, const Color& color, float line_width, float z_index
) {
	DrawPolygonHollowImpl(
		polygon.vertices.data(), polygon.vertices.size(), color.Normalized(), line_width, z_index
	);
}

void Renderer::DrawCircleFilled(
	const V2_float& position, float radius, const Color& color, float fade, float z_index
) {
	DrawEllipseFilledImpl(position, { radius, radius }, color.Normalized(), fade, z_index);
}

void Renderer::DrawCircleFilled(
	const Circle<float>& circle, const Color& color, float fade, float z_index
) {
	DrawEllipseFilledImpl(
		circle.center, { circle.radius, circle.radius }, color.Normalized(), fade, z_index
	);
}

void Renderer::DrawCircleHollow(
	const V2_float& position, float radius, const Color& color, float line_width, float fade,
	float z_index
) {
	DrawEllipseHollowImpl(
		position, { radius, radius }, color.Normalized(), line_width, fade, z_index
	);
}

void Renderer::DrawCircleHollow(
	const Circle<float>& circle, const Color& color, float line_width, float fade, float z_index
) {
	DrawEllipseHollowImpl(
		circle.center, { circle.radius, circle.radius }, color.Normalized(), line_width, fade,
		z_index
	);
}

// Rotation in degrees.
void Renderer::DrawRoundedRectangleFilled(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	Origin draw_origin, float rotation, const V2_float& rotation_center, float z_index
) {
	DrawRoundedRectangleFilledImpl(
		position, size, radius, color.Normalized(), draw_origin, rotation, rotation_center, z_index
	);
}

// Rotation in degrees.
void Renderer::DrawRoundedRectangleFilled(
	const RoundedRectangle<float>& rounded_rectangle, const Color& color, float rotation,
	const V2_float& rotation_center, float z_index
) {
	DrawRoundedRectangleFilledImpl(
		rounded_rectangle.pos, rounded_rectangle.size, rounded_rectangle.radius, color.Normalized(),
		rounded_rectangle.origin, rotation, rotation_center, z_index
	);
}

// Rotation in degrees.
void Renderer::DrawRoundedRectangleHollow(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	Origin draw_origin, float line_width, float rotation, const V2_float& rotation_center,
	float z_index
) {
	DrawRoundedRectangleHollowImpl(
		position, size, radius, color.Normalized(), draw_origin, line_width, rotation,
		rotation_center, z_index
	);
}

// Rotation in degrees.
void Renderer::DrawRoundedRectangleHollow(
	const RoundedRectangle<float>& rounded_rectangle, const Color& color, float line_width,
	float rotation, const V2_float& rotation_center, float z_index
) {
	DrawRoundedRectangleHollowImpl(
		rounded_rectangle.pos, rounded_rectangle.size, rounded_rectangle.radius, color.Normalized(),
		rounded_rectangle.origin, line_width, rotation, rotation_center, z_index
	);
}

void Renderer::DrawEllipseFilled(
	const V2_float& position, const V2_float& radius, const Color& color, float fade, float z_index
) {
	DrawEllipseFilledImpl(position, radius, color.Normalized(), fade, z_index);
}

void Renderer::DrawEllipseFilled(
	const Ellipse<float>& ellipse, const Color& color, float fade, float z_index
) {
	DrawEllipseFilledImpl(ellipse.center, ellipse.radius, color.Normalized(), fade, z_index);
}

void Renderer::DrawEllipseHollow(
	const V2_float& position, const V2_float& radius, const Color& color, float line_width,
	float fade, float z_index
) {
	DrawEllipseHollowImpl(position, radius, color.Normalized(), line_width, fade, z_index);
}

void Renderer::DrawEllipseHollow(
	const Ellipse<float>& ellipse, const Color& color, float line_width, float fade, float z_index
) {
	DrawEllipseHollowImpl(
		ellipse.center, ellipse.radius, color.Normalized(), line_width, fade, z_index
	);
}

// Angles in degrees.
void Renderer::DrawArcFilled(
	const V2_float& position, float arc_radius, float start_angle, float end_angle,
	const Color& color, float z_index
) {
	DrawArcFilledImpl(position, arc_radius, start_angle, end_angle, color.Normalized(), z_index);
}

void Renderer::DrawArcFilled(const Arc<float>& arc, const Color& color, float z_index) {
	DrawArcFilledImpl(
		arc.center, arc.radius, arc.start_angle, arc.end_angle, color.Normalized(), z_index
	);
}

// Angles in degrees.
void Renderer::DrawArcHollow(
	const V2_float& position, float arc_radius, float start_angle, float end_angle,
	const Color& color, float line_width, float z_index
) {
	DrawArcHollowImpl(
		position, arc_radius, start_angle, end_angle, color.Normalized(), line_width, z_index
	);
}

void Renderer::DrawArcHollow(
	const Arc<float>& arc, const Color& color, float line_width, float z_index
) {
	DrawArcHollowImpl(
		arc.center, arc.radius, arc.start_angle, arc.end_angle, color.Normalized(), line_width,
		z_index
	);
}

void Renderer::DrawCapsuleFilled(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float fade,
	float z_index
) {
	DrawCapsuleFilledImpl(p0, p1, radius, color.Normalized(), fade, z_index);
}

void Renderer::DrawCapsuleFilled(
	const Capsule<float>& capsule, const Color& color, float fade, float z_index
) {
	DrawCapsuleFilledImpl(
		capsule.segment.a, capsule.segment.b, capsule.radius, color.Normalized(), fade, z_index
	);
}

void Renderer::DrawCapsuleHollow(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float line_width,
	float fade, float z_index
) {
	DrawCapsuleHollowImpl(p0, p1, radius, color.Normalized(), line_width, fade, z_index);
}

void Renderer::DrawCapsuleHollow(
	const Capsule<float>& capsule, const Color& color, float line_width, float fade, float z_index
) {
	DrawCapsuleHollowImpl(
		capsule.segment.a, capsule.segment.b, capsule.radius, color.Normalized(), line_width, fade,
		z_index
	);
}

} // namespace ptgn