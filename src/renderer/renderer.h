#pragma once

#include <array>
#include <map>

#include "protegon/buffer.h"
#include "protegon/color.h"
#include "protegon/matrix4.h"
#include "protegon/shader.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"
#include "protegon/vector3.h"
#include "protegon/vector4.h"
#include "protegon/vertex_array.h"
#include "renderer/origin.h"

// TODO: Currently z_index is not a reliable way of layering drawn objects as it only pertains to a
// single batch type (i.e. quads and circles have their own z_indexs). This can result in unexpected
// behavior if for example the user tries to draw a hollow and filled triangle with varying
// z_indexes as one uses the line batch and one uses the triangle batch. Not sure how to fix this
// while maintaining batching and permitting alpha blending (depth testing requires turning alpha
// blending off).
// TODO: If batch size is very small, z_indexes get drawn in 2 calls which completely defeats the
// purpose of it.

// clang-format off
#define PTGN_SHADER_STRINGIFY_MACRO(x) PTGN_STRINGIFY_MACRO(x)

#ifdef __EMSCRIPTEN__
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND_MACRO(shader/es/)PTGN_EXPAND_MACRO(file))
#else
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND_MACRO(shader/core/)PTGN_EXPAND_MACRO(file))
#endif
// clang-format on

namespace ptgn {

template <typename T>
struct Triangle;

template <typename T>
struct Circle;

template <typename T>
struct Arc;

template <typename T>
struct Ellipse;

template <typename T>
struct Segment;

template <typename T>
struct Capsule;

template <typename T>
struct Line;

template <typename T>
struct Rectangle;

template <typename T>
struct RoundedRectangle;

struct Polygon;

class CameraManager;

struct QuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
};

struct CircleVertex {
	glsl::vec3 position;
	glsl::vec3 local_position;
	glsl::vec4 color;
	glsl::float_ line_width;
	glsl::float_ fade;
};

struct ColorVertex {
	glsl::vec3 position;
	glsl::vec4 color;
};

namespace impl {

[[nodiscard]] float TriangulateArea(const V2_float* contour, std::size_t count);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
);

[[nodiscard]] std::vector<Triangle<float>> TriangulateProcess(
	const V2_float* contour, std::size_t count
);

class RendererData;

template <typename TVertex, std::size_t V>
struct ShapeVertices {
public:
	constexpr static std::size_t count{ V };

	ShapeVertices() = default;

	// Takes in normalized color.
	ShapeVertices(
		const std::array<V2_float, count>& positions, float z_index, const V4_float& color
	) {
		PTGN_ASSERT(color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f && color.w >= 0.0f);
		PTGN_ASSERT(color.x <= 1.0f && color.y <= 1.0f && color.z <= 1.0f && color.w <= 1.0f);
		for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
			vertices_[i].position = { positions[i].x, positions[i].y, z_index };
			vertices_[i].color	  = { color.x, color.y, color.z, color.w };
		}
	}

protected:
	std::array<TVertex, count> vertices_{};
};

struct QuadVertices : public ShapeVertices<QuadVertex, 4> {
	using ShapeVertices::ShapeVertices;

	QuadVertices(
		const std::array<V2_float, count>& positions, float z_index, const V4_float& color,
		const std::array<V2_float, count>& tex_coords, float texture_index
	);
};

struct CircleVertices : public ShapeVertices<CircleVertex, 4> {
	using ShapeVertices::ShapeVertices;

	CircleVertices(
		const std::array<V2_float, count>& positions, float z_index, const V4_float& color,
		float line_width, float fade
	);
};

struct TriangleVertices : public ShapeVertices<ColorVertex, 3> {
	using ShapeVertices::ShapeVertices;
};

struct LineVertices : public ShapeVertices<ColorVertex, 2> {
	using ShapeVertices::ShapeVertices;
};

struct PointVertices : public ShapeVertices<ColorVertex, 1> {
	using ShapeVertices::ShapeVertices;
};

void OffsetVertices(std::array<V2_float, 4>& vertices, const V2_float& size, Origin draw_origin);

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);

// Rotation angle in radians.
void RotateVertices(
	std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
	float rotation_radians, const V2_float& rotation_center
);

// Rotation angle in radians.
[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(
	const V2_float& position, const V2_float& size, Origin draw_origin, float rotation_radians,
	const V2_float& rotation_center
);

class Batch;

template <typename TVertices, std::size_t IndexCount>
class BatchData {
public:
	BatchData(RendererData* renderer);

	using vertices = TVertices;

	[[nodiscard]] bool IsAvailable() const;

	[[nodiscard]] TVertices& Get();

	void Flush();

	void Clear();

private:
	friend class Batch;

	void SetupBuffer(
		PrimitiveMode type, const impl::InternalBufferLayout& layout, std::size_t vertex_count,
		const IndexBuffer& index_buffer
	);
	void UpdateBuffer();

	void PrepareBuffer();

	[[nodiscard]] bool IsFlushed() const;

	void Draw();

protected:
	RendererData* renderer_{ nullptr };
	VertexArray array_;
	std::vector<TVertices> data_;
};

class TextureBatchData : public BatchData<QuadVertices, 6> {
public:
	TextureBatchData() = default;

	TextureBatchData(RendererData* renderer, std::size_t max_texture_slots);

	void BindTextures();

	[[nodiscard]] bool IsAvailable() const;

	// @return pair<texture_index, texture_index_found>
	// If texture_index_found == false, texture_index will be 0.
	// Otherwise, texture index returned is always > 0.
	[[nodiscard]] std::pair<std::size_t, bool> GetTextureIndex(const Texture& t);

	[[nodiscard]] std::size_t GetTextureSlotCapacity() const;

	void Clear();

private:
	[[nodiscard]] bool HasAvailableTextureSlot() const;

	std::vector<Texture> textures_;
};

enum class BatchType {
	Quad,
	Circle,
	Triangle,
	Line,
	Point,
};

class Batch {
public:
	Batch() = default;
	Batch(RendererData* renderer);

	[[nodiscard]] bool IsFlushed(BatchType type);

	void Flush(BatchType type);

	[[nodiscard]] bool IsAvailable(BatchType type) const;

	void Clear();

	TextureBatchData quad_;
	BatchData<CircleVertices, 6> circle_;
	BatchData<TriangleVertices, 3> triangle_;
	BatchData<LineVertices, 2> line_;
	BatchData<PointVertices, 1> point_;
};

class RendererData {
public:
	RendererData()								 = default;
	~RendererData()								 = default;
	RendererData(const RendererData&)			 = delete;
	RendererData(RendererData&&)				 = default;
	RendererData& operator=(const RendererData&) = delete;
	RendererData& operator=(RendererData&&)		 = default;

	void Init();

	M4_float view_projection_{ 1.0f };

	bool new_view_projection_{ false };

	Shader quad_shader_;
	Shader circle_shader_;
	Shader color_shader_;

	IndexBuffer quad_ib_;
	IndexBuffer triangle_ib_;
	IndexBuffer line_ib_;
	IndexBuffer point_ib_;

	impl::InternalBufferLayout quad_layout{
		BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>{}
	};

	impl::InternalBufferLayout circle_layout{
		BufferLayout<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>{}
	};

	impl::InternalBufferLayout color_layout{ BufferLayout<glsl::vec3, glsl::vec4>{} };

	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	std::vector<Batch> opaque_batches_;

	// Key: z_index, Value: Batches for that z_index.
	std::map<std::int64_t, std::vector<Batch>> transparent_batches_;

	[[nodiscard]] Shader& GetShader(BatchType type);

	void AddQuad(
		const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
		const std::array<V2_float, 4>& tex_coords, const Texture& t
	);
	void AddCircle(
		const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
		float line_width, float fade
	);
	void AddTriangle(
		const V2_float& a, const V2_float& b, const V2_float& c, float z_index,
		const V4_float& color
	);
	void AddLine(const V2_float& p0, const V2_float& p1, float z_index, const V4_float& color);
	void AddPoint(const V2_float& position, float z_index, const V4_float& color);

	void SetupShaders();

	void Flush();

	[[nodiscard]] static std::array<V2_float, 4> GetTextureCoordinates(
		const V2_float& source_position, V2_float source_size, const V2_float& texture_size,
		Flip flip
	);

private:
	void FlushTransparentBatches();
	void FlushOpaqueBatches();

	void FlushBatches(std::vector<Batch>& batches);

	[[nodiscard]] std::vector<Batch>& GetBatchGroup(float alpha, float z_index);

	[[nodiscard]] Batch& GetBatch(BatchType type, std::vector<Batch>& batch_group);

	// @return pair<batch reference, texture index>
	[[nodiscard]] std::pair<Batch&, std::size_t> GetTextureBatch(
		BatchType type, std::vector<Batch>& batch_group, const Texture& t
	);
};

} // namespace impl

class Renderer {
private:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

public:
	void Clear() const;

	void Present();

	void Flush();

	void DrawElements(const VertexArray& va, std::size_t index_count);
	void DrawArrays(const VertexArray& va, std::size_t vertex_count);

	/*
	 * @param source_position Top left pixel to start drawing texture from within the texture
	 * (defaults to { 0, 0 }).
	 * @param source_size Number of pixels of the texture to draw (defaults to {} which corresponds
	 * to the remaining texture size to the bottom right of source_position).
	 * @param draw_origin Relative to destination_position the direction from which the texture is
	 * @param flip Mirror the texture along an axis (default to Flip::None).
	 * @param rotation_radians Angle in radians to rotate the texture (defaults to 0).
	 * @param rotation_center Fraction of the source_size around which the texture is rotated.
	 * (Defaults to { 0.5f, 0.5f } which corresponds to the center of the texture).
	 * @param z_index Z-coordinate to draw the texture at (-1.0f to 1.0f).
	 * drawn.
	 */
	void DrawTexture(
		const Texture& texture, const V2_float& destination_position,
		const V2_float& destination_size, const V2_float& source_position = {},
		V2_float source_size = {}, Origin draw_origin = Origin::Center, Flip flip = Flip::None,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f, const Color& tint_color = color::White
	);

	void DrawPoint(
		const V2_float& position, const Color& color, float radius = 1.0f, float z_index = 0.0f
	);

	void DrawLine(
		const V2_float& p0, const V2_float& p1, const Color& color, float line_width = 1.0f,
		float z_index = 0.0f
	);

	void DrawLine(
		const Line<float>& line, const Color& color, float line_width = 1.0f, float z_index = 0.0f
	);

	void DrawTriangleFilled(
		const V2_float& a, const V2_float& b, const V2_float& c, const Color& color,
		float z_index = 0.0f
	);

	void DrawTriangleHollow(
		const V2_float& a, const V2_float& b, const V2_float& c, const Color& color,
		float line_width = 1.0f, float z_index = 0.0f
	);

	void DrawTriangleFilled(
		const Triangle<float>& triangle, const Color& color, float z_index = 0.0f
	);

	void DrawTriangleHollow(
		const Triangle<float>& triangle, const Color& color, float line_width = 1.0f,
		float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRectangleFilled(
		const V2_float& position, const V2_float& size, const Color& color,
		Origin draw_origin = Origin::Center, float rotation_radians = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRectangleFilled(
		const Rectangle<float>& rectangle, const Color& color, float rotation_radians = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRectangleHollow(
		const V2_float& position, const V2_float& size, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = 1.0f, float rotation_radians = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRectangleHollow(
		const Rectangle<float>& rectangle, const Color& color, float line_width = 1.0f,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f
	);

	void DrawPolygonFilled(
		const V2_float* vertices, std::size_t vertex_count, const Color& color, float z_index = 0.0f
	);

	void DrawPolygonFilled(const Polygon& polygon, const Color& color, float z_index = 0.0f);

	void DrawPolygonHollow(
		const V2_float* vertices, std::size_t vertex_count, const Color& color,
		float line_width = 1.0f, float z_index = 0.0f
	);

	void DrawPolygonHollow(
		const Polygon& polygon, const Color& color, float line_width = 1.0f, float z_index = 0.0f
	);

	void DrawCircleFilled(
		const V2_float& position, float radius, const Color& color, float fade = 0.005f,
		float z_index = 0.0f
	);

	void DrawCircleFilled(
		const Circle<float>& circle, const Color& color, float fade = 0.005f, float z_index = 0.0f
	);

	void DrawCircleHollow(
		const V2_float& position, float radius, const Color& color, float line_width = 1.0f,
		float fade = 0.005f, float z_index = 0.0f
	);

	void DrawCircleHollow(
		const Circle<float>& circle, const Color& color, float line_width = 1.0f,
		float fade = 0.005f, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRoundedRectangleFilled(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		Origin draw_origin = Origin::Center, float rotation_radians = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRoundedRectangleFilled(
		const RoundedRectangle<float>& rounded_rectangle, const Color& color,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRoundedRectangleHollow(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = 1.0f, float rotation_radians = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void DrawRoundedRectangleHollow(
		const RoundedRectangle<float>& rounded_rectangle, const Color& color,
		float line_width = 1.0f, float rotation_radians = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	void DrawEllipseFilled(
		const V2_float& position, const V2_float& radius, const Color& color, float fade = 0.005f,
		float z_index = 0.0f
	);

	void DrawEllipseFilled(
		const Ellipse<float>& ellipse, const Color& color, float fade = 0.005f, float z_index = 0.0f
	);

	void DrawEllipseHollow(
		const V2_float& position, const V2_float& radius, const Color& color,
		float line_width = 1.0f, float fade = 0.005f, float z_index = 0.0f
	);

	void DrawEllipseHollow(
		const Ellipse<float>& ellipse, const Color& color, float line_width = 1.0f,
		float fade = 0.005f, float z_index = 0.0f
	);

	// Angles in radians, counter-clockwise from the right.
	void DrawArcFilled(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		const Color& color, float z_index = 0.0f
	);

	void DrawArcFilled(const Arc<float>& arc, const Color& color, float z_index = 0.0f);

	// Angles in radians, counter-clockwise from the right.
	void DrawArcHollow(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		const Color& color, float line_width = 1.0f, float z_index = 0.0f
	);

	void DrawArcHollow(
		const Arc<float>& arc, const Color& color, float line_width = 1.0f, float z_index = 0.0f
	);

	void DrawCapsuleFilled(
		const V2_float& p0, const V2_float& p1, float radius, const Color& color,
		float fade = 0.005f, float z_index = 0.0f
	);

	void DrawCapsuleFilled(
		const Capsule<float>& capsule, const Color& color, float fade = 0.005f, float z_index = 0.0f
	);

	void DrawCapsuleHollow(
		const V2_float& p0, const V2_float& p1, float radius, const Color& color,
		float line_width = 1.0f, float fade = 0.005f, float z_index = 0.0f
	);

	void DrawCapsuleHollow(
		const Capsule<float>& capsule, const Color& color, float line_width = 1.0f,
		float fade = 0.005f, float z_index = 0.0f
	);

	void SetBlendMode(BlendMode mode);
	BlendMode GetBlendMode() const;

	void SetClearColor(const Color& color);
	Color GetClearColor() const;

	void SetViewport(const V2_int& size);

	void UpdateViewProjection(const M4_float& view_projection);

private:
	friend class CameraManager;
	friend class Game;

	void Init();
	void Shutdown();
	void Reset();

	void DrawTextureImpl(
		const std::array<V2_float, 4>& vertices, const Texture& t,
		const std::array<V2_float, 4>& tex_coords, const V4_float& tint_color, float z
	);

	void DrawPointImpl(
		const V2_float& position, const V4_float& color, float radius, float z_index
	);

	void DrawLineImpl(
		const V2_float& p0, const V2_float& p1, const V4_float& color, float line_width,
		float z_index
	);

	void DrawTriangleFilledImpl(
		const V2_float& a, const V2_float& b, const V2_float& c, const V4_float& color,
		float z_index
	);

	void DrawTriangleHollowImpl(
		const V2_float& a, const V2_float& b, const V2_float& c, const V4_float& color,
		float line_width, float z_index
	);

	void DrawRectangleFilledImpl(
		const std::array<V2_float, 4>& vertices, const V4_float& color, float z_index
	);

	void DrawRectangleHollowImpl(
		const std::array<V2_float, 4>& vertices, const V4_float& color, float line_width,
		float z_index
	);

	void DrawPolygonFilledImpl(
		const Triangle<float>* triangles, std::size_t triangle_count, const V4_float& col, float z
	);

	void DrawPolygonHollowImpl(
		const V2_float* vertices, std::size_t vertex_count, const V4_float& color, float line_width,
		float z_index
	);

	void DrawRoundedRectangleFilledImpl(
		const V2_float& position, const V2_float& size, float radius, const V4_float& color,
		Origin origin, float rotation_radians, const V2_float& rotation_center, float z_index
	);

	void DrawRoundedRectangleHollowImpl(
		const V2_float& position, const V2_float& size, float radius, const V4_float& color,
		Origin origin, float line_width, float rotation_radians, const V2_float& rotation_center,
		float z_index
	);

	void DrawEllipseFilledImpl(
		const V2_float& position, const V2_float& radius, const V4_float& color, float fade,
		float z_index
	);

	// TODO: Fix ellipse line width being uneven between x and y axes (currently it chooses the
	// smaller radius axis as the relative radius).
	void DrawEllipseHollowImpl(
		const V2_float& position, const V2_float& radius, const V4_float& color, float line_width,
		float fade, float z_index
	);

	void DrawArcImpl(
		const V2_float& p, float arc_radius, float start_angle, float end_angle,
		const V4_float& col, float lw, float z, bool filled
	);

	void DrawArcFilledImpl(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		const V4_float& color, float z_index
	);

	void DrawArcHollowImpl(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		const V4_float& color, float line_width, float z_index
	);

	void DrawCapsuleFilledImpl(
		const V2_float& p0, const V2_float& p1, float radius, const V4_float& color, float fade,
		float z_index
	);

	// TODO: Fix artefacts in capsule line width at larger radii.
	void DrawCapsuleHollowImpl(
		const V2_float& p0, const V2_float& p1, float radius, const V4_float& color,
		float line_width, float fade, float z_index
	);

	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Blend };
	V2_int viewport_size_;
	impl::RendererData data_;
};

} // namespace ptgn