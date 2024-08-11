#include "renderer.h"

#include <numeric>

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

float TriangulateArea(const V2_float* contour, std::size_t count) {
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

bool TriangulateSnip(const V2_float* contour, int u, int v, int w, int n, int* V) {
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
	/* allocate and initialize list of Vertices in polygon */
	std::vector<Triangle<float>> result;

	int n = static_cast<int>(count);
	if (n < 3) {
		return result;
	}

	int* V = new int[n];

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

	delete V;

	return result;
}

void QuadData::Add(
	const std::array<V2_float, vertex_count>& vertices, float z_index, const V4_float& color,
	const std::array<V2_float, vertex_count>& tex_coords, float texture_index, float tiling_factor
) {
	ShapeData::Add(vertices, z_index, color);
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].tex_coord	   = { tex_coords[i].x, tex_coords[i].y };
		vertices_[i].tex_index	   = { texture_index };
		vertices_[i].tiling_factor = { tiling_factor };
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

	IndexBuffer triangle_index_buffer{ GetIndices(
		[](auto& indices, auto i, auto offset) {
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;
		},
		triangle_.max_indices_, TriangleData::vertex_count, TriangleData::index_count
	) };

	quad_.batch_.resize(quad_.max_vertices_);
	circle_.batch_.resize(circle_.max_vertices_);
	line_.batch_.resize(line_.max_vertices_);
	triangle_.batch_.resize(triangle_.max_vertices_);

	quad_.buffer_ = VertexBuffer(
		quad_.batch_,
		BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::float_>{},
		BufferUsage::DynamicDraw
	);

	triangle_.buffer_ = VertexBuffer(
		triangle_.batch_, BufferLayout<glsl::vec3, glsl::vec4>{}, BufferUsage::DynamicDraw
	);

	circle_.buffer_ = VertexBuffer(
		circle_.batch_,
		BufferLayout<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>{},
		BufferUsage::DynamicDraw
	);

	line_.buffer_ = VertexBuffer(
		line_.batch_, BufferLayout<glsl::vec3, glsl::vec4>{}, BufferUsage::DynamicDraw
	);

	quad_.array_	 = { PrimitiveMode::Triangles, quad_.buffer_, quad_index_buffer };
	circle_.array_	 = { PrimitiveMode::Triangles, circle_.buffer_, quad_index_buffer };
	line_.array_	 = { PrimitiveMode::Lines, line_.buffer_, line_index_buffer };
	triangle_.array_ = { PrimitiveMode::Triangles, triangle_.buffer_, triangle_index_buffer };
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

	quad_.shader_ = Shader(
		"resources/shader/renderer_quad_vertex.glsl", "resources/shader/renderer_quad_fragment.glsl"
	);

	circle_.shader_ = Shader(
		"resources/shader/renderer_circle_vertex.glsl",
		"resources/shader/renderer_circle_fragment.glsl"
	);

	line_.shader_ = Shader(
		"resources/shader/renderer_line_vertex.glsl", "resources/shader/renderer_line_fragment.glsl"
	);

	triangle_.shader_ = line_.shader_;

	std::vector<std::int32_t> samplers(max_texture_slots_);

	for (std::uint32_t i{ 0 }; i < samplers.size(); ++i) {
		samplers[i] = i;
	}

	quad_.shader_.Bind();
	quad_.shader_.SetUniform("u_Textures", samplers.data(), samplers.size());
}

void RendererData::Flush() {
	if (!quad_.IsFlushed()) {
		BindTextures();
		quad_.Draw();
		stats_.draw_calls++;
	}
	if (!triangle_.IsFlushed()) {
		triangle_.Draw();
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
	quad_count	   = 0;
	triangle_count = 0;
	circle_count   = 0;
	line_count	   = 0;
	draw_calls	   = 0;
};

void RendererData::Stats::Print() {
	PTGN_INFO(
		"Draw Calls: ", draw_calls, ", Quads: ", quad_count, ", Triangles: ", triangle_count,
		", Circles: ", circle_count, ", Lines: ", line_count
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

void Renderer::DrawArray(const VertexArray& vertex_array) {
	PTGN_ASSERT(vertex_array.IsValid(), "Cannot submit invalid vertex array for rendering");
	GLRenderer::DrawElements(vertex_array);
	data_.stats_.draw_calls++;
}

void Renderer::DrawTriangleFilled(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float z_index
) {
	data_.triangle_.AdvanceBatch();

	PTGN_ASSERT(data_.triangle_.index_ != -1);
	PTGN_ASSERT(data_.triangle_.index_ < data_.triangle_.batch_.size());

	data_.triangle_.batch_[data_.triangle_.index_].Add({ a, b, c }, z_index, color);

	data_.stats_.triangle_count++;
}

void Renderer::DrawTriangleHollow(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float line_width,
	float z_index
) {
	std::array<V2_float, impl::TriangleData::vertex_count> vertices{ a, b, c };

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		DrawLine(vertices[i], vertices[(i + 1) % vertices.size()], color, line_width, z_index);
	}
}

void Renderer::DrawTexture(
	const V2_float& destination_position, const V2_float& destination_size, const Texture& texture,
	const V2_float& source_position, V2_float source_size, float rotation,
	const V2_float& rotation_center, Flip flip, Origin draw_origin, float z_index,
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

void Renderer::DrawRectangleFilled(
	const V2_float& position, const V2_float& size, const Color& color, float rotation,
	const V2_float& rotation_center, Origin draw_origin, float z_index
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
	const V2_float& rotation_center, float line_width, Origin draw_origin, float z_index
) {
	auto vertices = impl::GetQuadVertices(position, size, draw_origin, rotation, rotation_center);

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		DrawLine(vertices[i], vertices[(i + 1) % vertices.size()], color, line_width, z_index);
	}
}

void Renderer::DrawRoundedRectangleFilled(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	float rotation, const V2_float& rotation_center, Origin origin, float z_index
) {
	// TODO: Implement.
	// TODO: Batch a filled quad, and 4 filled capsules.
	//  o -  o
	//  l [] l
	//  o -  o
	// ...
}

void Renderer::DrawRoundedRectangleHollow(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	float rotation, const V2_float& rotation_center, float line_width, Origin origin, float z_index
) {
	// TODO: Implement.
	/*int tmp;
	int xx1, xx2;
	int yy1, yy2;

	PTGN_ASSERT(r >= 0, "Cannot draw thick rounded rectangle with negative radius");

	if (r <= 1) {
		DrawThickRectangleImpl(renderer, x, y, x + w, y + h, pixel_thickness);
		return;
	}

	int x2 = x + w;
	int y2 = y + h;

	if (x == x2) {
		if (y == y2) {
			DrawPointImpl(renderer, x, y);
			return;
		} else {
			DrawThickVerticalLineImpl(renderer, x, y, y2, pixel_thickness);
			return;
		}
	} else {
		if (y == y2) {
			DrawThickHorizontalLineImpl(renderer, x, x2, y, pixel_thickness);
			return;
		}
	}

	if (x > x2) {
		tmp = x;
		x	= x2;
		x2	= tmp;
	}

	if (y > y2) {
		tmp = y;
		y	= y2;
		y2	= tmp;
	}

	if (2 * r > w) {
		r = w / 2;
	}
	if (2 * r > h) {
		r = h / 2;
	}

	xx1 = x + r;
	xx2 = x2 - r;
	yy1 = y + r;
	yy2 = y2 - r;

	DrawThickArcImpl(renderer, xx1, yy1, r, 180, 270, pixel_thickness + 1);
	DrawThickArcImpl(renderer, xx2, yy1, r, 270, 360, pixel_thickness + 1);
	DrawThickArcImpl(renderer, xx1, yy2, r, 90, 180, pixel_thickness + 1);
	DrawThickArcImpl(renderer, xx2, yy2, r, 0, 90, pixel_thickness + 1);

	if (xx1 <= xx2) {
		DrawThickHorizontalLineImpl(renderer, xx1, xx2, y, pixel_thickness);
		DrawThickHorizontalLineImpl(renderer, xx1, xx2, y2, pixel_thickness);
	}

	if (yy1 <= yy2) {
		DrawThickVerticalLineImpl(renderer, x, yy1, yy2, pixel_thickness);
		DrawThickVerticalLineImpl(renderer, x2, yy1, yy2, pixel_thickness);
	}*/
}

void Renderer::DrawPoint(
	const V2_float& position, const Color& color, float radius, float z_index
) {
	DrawCircleFilled(position, radius, color, 0.005f, z_index);
}

void Renderer::DrawCircleFilled(
	const V2_float& position, float radius, const Color& color, float fade, float z_index
) {
	DrawEllipseFilled(position, { radius, radius }, color, fade, z_index);
}

void Renderer::DrawCircleHollow(
	const V2_float& position, float radius, const Color& color, float line_width, float fade,
	float z_index
) {
	DrawEllipseHollow(position, { radius, radius }, color, line_width, fade, z_index);
}

void Renderer::DrawLine(
	const V2_float& p0, const V2_float& p1, const Color& color, float line_width, float z_index
) {
	PTGN_ASSERT(line_width >= 0.0f, "Cannot draw negative line width");

	if (line_width > 1.0f) {
		V2_float d{ p1 - p0 };
		float angle{ static_cast<float>(d.Angle()) };
		// TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
		V2_float center{ p0 + d * 0.5f };
		V2_float size{ d.Magnitude(), line_width };
		DrawRectangleFilled(center, size, color, angle, { 0.5f, 0.5f }, Origin::Center, z_index);
		return;
	}

	data_.line_.AdvanceBatch();

	PTGN_ASSERT(data_.line_.index_ != -1);
	PTGN_ASSERT(data_.line_.index_ < data_.line_.batch_.size());

	data_.line_.batch_[data_.line_.index_].Add({ p0, p1 }, z_index, color);

	data_.stats_.line_count++;
}

void Renderer::DrawEllipseFilled(
	const V2_float& position, const V2_float& radius, const Color& color, float fade, float z_index
) {
	DrawEllipseHollow(position, radius, color, 0.0f, fade, z_index);
}

void Renderer::DrawEllipseHollow(
	const V2_float& position, const V2_float& radius, const Color& color, float line_width,
	float fade, float z_index
) {
	PTGN_ASSERT(line_width >= 0.0f, "Cannot draw negative line width");

	data_.circle_.AdvanceBatch();

	const V2_float size{ radius.x * 2.0f, radius.y * 2.0f };

	auto vertices = impl::GetQuadVertices(position, size, Origin::Center, 0.0f, { 0.5f, 0.5f });

	PTGN_ASSERT(data_.circle_.index_ != -1);
	PTGN_ASSERT(data_.circle_.index_ < data_.circle_.batch_.size());

	// Internally line width for a filled rectangle is 1.0f and a completely hollow one is 0.0f, but
	// in the API the line width is expected in pixels.
	// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected bugs.
	line_width =
		NearlyEqual(line_width, 0.0f) ? 1.0f : fade + line_width / std::min(radius.x, radius.y);

	data_.circle_.batch_[data_.circle_.index_].Add(vertices, z_index, color, line_width, fade);

	data_.stats_.circle_count++;
}

void Renderer::DrawArcFilled(
	const V2_float& position, float arc_radius, const Color& color, float start_angle,
	float end_angle, float z_index
) {
	PTGN_ASSERT(arc_radius >= 0.0f, "Cannot draw solid arc with negative radius");

	float angle;
	float delta_angle;
	float dr;
	int numpoints, i;

	start_angle = RestrictAngle360(start_angle);
	end_angle	= RestrictAngle360(end_angle);

	if (NearlyEqual(arc_radius, 0.0f)) {
		DrawPoint(position, color, 1.0f, z_index);
		return;
	}

	dr			= arc_radius;
	delta_angle = 3.0f / dr;
	start_angle = DegToRad(start_angle);
	end_angle	= DegToRad(end_angle);
	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	numpoints = 2;

	angle = start_angle;
	while (angle < end_angle) {
		angle += delta_angle;
		numpoints++;
	}

	std::vector<V2_float> v(numpoints);
	v.at(0) = position;
	angle	= start_angle;
	v.at(1) = position + dr * V2_float{ std::cos(angle), std::sin(angle) };

	if (numpoints < 3) {
		DrawLine(v.at(0), v.at(1), color, 1.0f, z_index);
	} else {
		i	  = 2;
		angle = start_angle;
		while (angle < end_angle) {
			angle += delta_angle;
			angle  = std::min(angle, end_angle);
			v[i]   = position + dr * V2_float{ std::cos(angle), std::sin(angle) };
			i++;
		}

		DrawPolygonFilled(v.data(), v.size(), color, z_index);
	}
}

void Renderer::DrawArcHollow(
	const V2_float& position, float arc_radius, const Color& color, float start_angle,
	float end_angle, float line_width, float z_index
) {
	PTGN_ASSERT(arc_radius >= 0.0f, "Cannot draw thick arc with negative radius");

	start_angle = RestrictAngle360(start_angle);
	end_angle	= RestrictAngle360(end_angle);

	if (NearlyEqual(arc_radius, 0.0f)) {
		DrawPoint(position, color, 1.0f, z_index);
		return;
	}

	float delta_angle = two_pi<float> / arc_radius;

	start_angle = DegToRad(start_angle);
	end_angle	= DegToRad(end_angle);

	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	float arc = end_angle - start_angle;

	PTGN_ASSERT(arc >= 0.0f);

	std::size_t n = static_cast<std::size_t>(FastCeil(arc / delta_angle)) + 1;

	if (n > 1) {
		std::vector<V2_float> v(n);

		for (std::size_t i{ 0 }; i < n; i++) {
			float angle = start_angle + i * delta_angle;
			V2_float p{ position.x + arc_radius * std::cos(angle),
						position.y + arc_radius * std::sin(angle) };
			v[i] = p;
		}

		// Final line is skipped to prevent connecting the arc.
		for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
			DrawLine(v[i], v[i + 1], color, line_width, z_index);
		}
	}
}

void Renderer::DrawCapsuleFilled(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float fade,
	float z_index
) {
	DrawCircleFilled(p0, radius, color, fade, z_index);
	DrawCircleFilled(p1, radius, color, fade, z_index);
	DrawLine(p0, p1, color, radius * 2.0f, z_index);
}

void Renderer::DrawCapsuleHollow(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float line_width,
	float fade, float z_index
) {
	V2_float dir{ p1 - p0 };
	const float angle{ RadToDeg(RestrictAngle2Pi(dir.Angle() + half_pi<float>)) };
	const float dir2{ dir.Dot(dir) };

	V2_float tangent_r;

	// Note that dir2 is an int.
	if (NearlyEqual(dir2, 0.0f)) {
		DrawCircleHollow(p0, radius, color, line_width, fade, z_index);
		return;
	} else {
		V2_float tmp = dir.Skewed() / std::sqrt(dir2) * radius;
		tangent_r	 = { FastFloor(tmp.x), FastFloor(tmp.y) };
	}

	// Draw edge lines.
	DrawLine(p0 + tangent_r, p1 + tangent_r, color, line_width, z_index);
	DrawLine(p0 - tangent_r, p1 - tangent_r, color, line_width, z_index);

	// Draw edge arcs.
	DrawArcHollow(p0, radius, color, angle, angle + 180.0f, line_width, z_index);
	DrawArcHollow(p1, radius, color, angle + 180.0f, angle, line_width, z_index);
}

void Renderer::DrawPolygonFilled(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float z_index
) {
	auto triangles = impl::TriangulateProcess(vertices, vertex_count);

	for (const Triangle<float>& t : triangles) {
		data_.triangle_.AdvanceBatch();
		PTGN_ASSERT(data_.triangle_.index_ != -1);
		PTGN_ASSERT(data_.triangle_.index_ < data_.triangle_.batch_.size());
		data_.triangle_.batch_[data_.triangle_.index_].Add({ t.a, t.b, t.c }, z_index, color);
	}

	data_.stats_.triangle_count++;
}

void Renderer::DrawPolygonHollow(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float line_width,
	float z_index
) {
	for (std::size_t i{ 0 }; i < vertex_count; i++) {
		DrawLine(vertices[i], vertices[(i + 1) % vertex_count], color, line_width, z_index);
	}
}

Color Renderer::GetClearColor() const {
	return clear_color_;
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
}

} // namespace ptgn