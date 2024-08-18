#pragma once

#include <array>

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
	glsl::float_ tiling_factor;
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

[[nodiscard]] bool TriangulateSnip(const V2_float* contour, int u, int v, int w, int n, int* V);

[[nodiscard]] std::vector<Triangle<float>> TriangulateProcess(
	const V2_float* contour, std::size_t count
);

class RendererData;

template <typename TVertex, std::size_t V, std::size_t I>
struct ShapeData {
public:
	constexpr static std::size_t vertex_count{ V };
	constexpr static std::size_t index_count{ I };

	[[nodiscard]] float GetZIndex() const {
		return vertices_[0].position[2];
	}

	void Add(
		const std::array<V2_float, vertex_count>& vertices, float z_index, const V4_float& color
	) {
		for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
			vertices_[i].position = { vertices[i].x, vertices[i].y, z_index };
			vertices_[i].color	  = { color.x, color.y, color.z, color.w };
		}
	}

protected:
	std::array<TVertex, V> vertices_;
};

struct QuadData : public ShapeData<QuadVertex, 4, 6> {
	void Add(
		const std::array<V2_float, vertex_count>& vertices, float z_index, const V4_float& color,
		const std::array<V2_float, vertex_count>& tex_coords, float texture_index,
		float tiling_factor
	);
};

struct CircleData : public ShapeData<CircleVertex, 4, 6> {
	void Add(
		const std::array<V2_float, vertex_count>& vertices, float z_index, const V4_float& color,
		float line_width, float fade
	);
};

struct PointData : public ShapeData<ColorVertex, 1, 1> {};

struct LineData : public ShapeData<ColorVertex, 2, 2> {};

struct TriangleData : public ShapeData<ColorVertex, 3, 3> {};

void OffsetVertices(
	std::array<V2_float, QuadData::vertex_count>& vertices, const V2_float& size, Origin draw_origin
);

void FlipTextureCoordinates(
	std::array<V2_float, QuadData::vertex_count>& texture_coords, Flip flip
);

void RotateVertices(
	std::array<V2_float, QuadData::vertex_count>& vertices, const V2_float& position,
	const V2_float& size, float rotation, const V2_float& rotation_center
);

[[nodiscard]] std::array<V2_float, QuadData::vertex_count> GetQuadVertices(
	const V2_float& position, const V2_float& size, Origin draw_origin, float rotation,
	const V2_float& rotation_center
);

template <typename T>
class BatchData {
public:
	VertexArray array_;
	VertexBuffer buffer_;
	Shader shader_;
	std::vector<T> batch_;
	std::int32_t index_{ -1 };

	constexpr static const std::size_t batch_count_	 = 5000;
	constexpr static const std::size_t max_vertices_ = batch_count_ * T::vertex_count;
	constexpr static const std::size_t max_indices_	 = batch_count_ * T::index_count;

	void AdvanceBatch() {
		index_++;
		if (index_ + 1 >= batch_count_) {
			Draw();
			index_ = 0;
		}
	}

	[[nodiscard]] bool IsFlushed() const {
		return index_ == -1;
	}

	void Draw();
};

class RendererData {
public:
	RendererData();
	~RendererData() = default;

	std::uint32_t max_texture_slots_{ 0 };

	Texture white_texture_;

	std::vector<Texture> texture_slots_;
	std::uint32_t texture_index_{ 1 }; // 0 reserved for white texture

	BatchData<QuadData> quad_;
	BatchData<CircleData> circle_;
	BatchData<PointData> point_;
	BatchData<LineData> line_;
	BatchData<TriangleData> triangle_;

	float line_width_{ 2.0f };

	void SetupBuffers();
	void SetupTextureSlots();
	void SetupShaders();

	void Flush();

	void BindTextures() const;

	// @return Slot index of the texture or 0.0f if texture index is currently not in texture slots.
	float GetTextureIndex(const Texture& texture);

	[[nodiscard]] static std::array<V2_float, QuadData::vertex_count> GetTextureCoordinates(
		const V2_float& source_position, V2_float source_size, const V2_float& texture_size
	);

	[[nodiscard]] static std::vector<IndexBuffer::IndexType> GetIndices(
		const std::function<void(std::vector<IndexBuffer::IndexType>&, std::size_t, std::uint32_t)>&
			func,
		std::size_t max_indices, std::size_t vertex_count, std::size_t index_count
	);

	struct Stats {
		std::int64_t quad_count{ 0 };
		std::int64_t circle_count{ 0 };
		std::int64_t point_count{ 0 };
		std::int64_t line_count{ 0 };
		std::int64_t triangle_count{ 0 };

		std::int64_t draw_calls{ 0 };

		void Reset();

		void Print();
	};

	Stats stats_;
};

} // namespace impl

class Renderer {
private:
	Renderer();
	~Renderer();
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

public:
	void Clear() const;

	void Present(bool print_stats = false);

	void Flush();

	void DrawArray(const VertexArray& vertex_array);

	/*
	 * @param source_position Top left pixel to start drawing texture from within the texture
	 * (defaults to { 0, 0 }).
	 * @param source_size Number of pixels of the texture to draw (defaults to {} which corresponds
	 * to the remaining texture size to the bottom right of source_position).
	 * @param rotation Degrees to rotate the texture (defaults to 0).
	 * @param rotation_center Fraction of the source_size around which the texture is rotated
	 * (defaults to { 0.5f, 0.5f } which corresponds to the center of the texture).
	 * @param flip Mirror the texture along an axis (default to Flip::None).
	 * @param z_index Z-coordinate to draw the texture at (-1.0f to 1.0f).
	 * @param draw_origin Relative to destination_position the direction from which the texture is
	 * drawn.
	 */
	void DrawTexture(
		const V2_float& destination_position, const V2_float& destination_size,
		const Texture& texture, const V2_float& source_position = {}, V2_float source_size = {},
		float rotation = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		Flip flip = Flip::None, Origin draw_origin = Origin::Center, float z_index = 0.0f,
		float tiling_factor = 1.0f, const Color& tint_color = color::White
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

	// Rotation in degrees.
	void DrawRectangleFilled(
		const V2_float& position, const V2_float& size, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, Origin draw_origin = Origin::Center,
		float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRectangleFilled(
		const Rectangle<float>& rectangle, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRectangleHollow(
		const V2_float& position, const V2_float& size, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float line_width = 1.0f,
		Origin draw_origin = Origin::Center, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRectangleHollow(
		const Rectangle<float>& rectangle, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float line_width = 1.0f,
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

	// Rotation in degrees.
	void DrawRoundedRectangleFilled(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		float rotation = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		Origin origin = Origin::Center, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRoundedRectangleFilled(
		const RoundedRectangle<float>& rounded_rectangle, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRoundedRectangleHollow(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		float rotation = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float line_width = 1.0f, Origin origin = Origin::Center, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRoundedRectangleHollow(
		const RoundedRectangle<float>& rounded_rectangle, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float line_width = 1.0f,
		float z_index = 0.0f
	);

	// Following functions Taken from:
	// https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c
	// (with modifications)

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

	// Angles in degrees.
	void DrawArcFilled(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		const Color& color, float z_index = 0.0f
	);

	void DrawArcFilled(const Arc<float>& arc, const Color& color, float z_index = 0.0f);

	// Angles in degrees.
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

private:
	friend class CameraManager;
	friend class Game;

	void StartBatch();

	void UpdateViewProjection(const M4_float& view_projection);

	Color clear_color_;
	BlendMode blend_mode_;
	V2_int viewport_size_;
	impl::RendererData data_;
};

} // namespace ptgn