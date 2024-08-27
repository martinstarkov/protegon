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

void RotateVertices(
	std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
	float rotation, const V2_float& rotation_center
);

[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(
	const V2_float& position, const V2_float& size, Origin draw_origin, float rotation,
	const V2_float& rotation_center
);

class Batch;

template <typename TVertices, std::size_t IndexCount>
class BatchData {
public:
	using vertices = TVertices;

	[[nodiscard]] bool IsAvailable() const {
		return data_.size() != data_.capacity();
	}

	[[nodiscard]] TVertices& Get() {
		PTGN_ASSERT(data_.size() + 1 <= data_.capacity());
		return data_.emplace_back(TVertices{});
	}

	void Flush() {
		if (!IsFlushed()) {
			Draw();
		}
	}

private:
	friend class Batch;

	void UpdateBuffer() {
		array_.GetVertexBuffer().SetSubData(
			data_.data(), static_cast<std::uint32_t>(data_.size()) * sizeof(TVertices)
		);
	}

	[[nodiscard]] bool IsFlushed() const {
		return data_.size() == 0;
	}

	void Draw();

protected:
	VertexArray array_;
	std::vector<TVertices> data_;
};

class TextureBatchData : public BatchData<QuadVertices, 6> {
public:
	TextureBatchData() = default;

	TextureBatchData(std::size_t max_texture_slots) {
		// First texture slot is reserved for the empty white texture.
		textures_.reserve(max_texture_slots - 1);
	}

	void BindTextures() {
		for (std::uint32_t i{ 0 }; i < textures_.size(); i++) {
			// Save first texture slot for empty white texture.
			auto slot{ i + 1 };
			textures_[i].Bind(slot);
			/*PTGN_LOG(
				"Active Slot: ", Texture::GetActiveSlot(),
				", Bound Texture: ", Texture::GetBoundId()
			);*/
		}
	}

	[[nodiscard]] bool IsAvailable() const {
		return data_.size() != data_.capacity();
	}

	// @return pair<texture_index, texture_index_found>
	// If texture_index_found == false, texture_index will be 0.
	// Otherwise, texture index returned is always > 0.
	[[nodiscard]] std::pair<std::size_t, bool> GetTextureIndex(const Texture& t) {
		// Texture exists in batch, therefore do not readd it.
		for (std::size_t i = 0; i < textures_.size(); i++) {
			if (textures_[i] == t) {
				// First texture index is white texture.
				return { i + 1, true };
			}
		}
		// Texture does not exist in batch but can be added.
		if (HasAvailableTextureSlot()) {
			auto index{ textures_.size() + 1 };
			textures_.emplace_back(t);
			return { index, true };
		}
		// Texture does not exist in batch and batch is full.
		return { 0, false };
	}

	[[nodiscard]] std::size_t GetTextureSlotCapacity() const {
		return textures_.capacity();
	}

private:
	[[nodiscard]] bool HasAvailableTextureSlot() const {
		return textures_.size() != textures_.capacity();
	}

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

	void Flush(BatchType type) {
		switch (type) {
			case BatchType::Quad: {
				quad_.BindTextures();
				quad_.Flush();
				break;
			}
			case BatchType::Triangle: {
				triangle_.Flush();
				break;
			}
			case BatchType::Line: {
				line_.Flush();
				break;
			}
			case BatchType::Circle: {
				circle_.Flush();
				break;
			}
			case BatchType::Point: {
				point_.Flush();
				break;
			}
			default: PTGN_ERROR("Failed to recognize batch type when flushing");
		}
	}

	[[nodiscard]] bool IsAvailable(BatchType type) const {
		switch (type) {
			case BatchType::Quad:	  return quad_.IsAvailable();
			case BatchType::Triangle: return triangle_.IsAvailable();
			case BatchType::Line:	  return line_.IsAvailable();
			case BatchType::Circle:	  return circle_.IsAvailable();
			case BatchType::Point:	  return point_.IsAvailable();
			default:				  PTGN_ERROR("Failed to identify batch type when checking availability");
		}
	}

	TextureBatchData quad_;
	BatchData<CircleVertices, 6> circle_;
	BatchData<TriangleVertices, 3> triangle_;
	BatchData<LineVertices, 2> line_;
	BatchData<PointVertices, 1> point_;

private:
	template <typename T>
	void SetArray(
		T& data, PrimitiveMode p, const impl::InternalBufferLayout& layout, const IndexBuffer& ib
	) {
		data.array_ = { p,
						VertexBuffer(
							data.data_.data(),
							static_cast<std::uint32_t>(
								data.data_.capacity() * layout.GetStride() * T::vertices::count
							),
							BufferUsage::DynamicDraw
						),
						layout, ib };
	}

	RendererData* renderer_{ nullptr };
};

class RendererData {
public:
	RendererData();
	~RendererData() = default;

	static constexpr std::size_t batch_capacity_{ 2000 };

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

	[[nodiscard]] Shader& GetShader(BatchType type) {
		switch (type) {
			case BatchType::Quad:	  return quad_shader_;
			case BatchType::Triangle:
			case BatchType::Line:
			case BatchType::Point:	  return color_shader_;
			case BatchType::Circle:	  return circle_shader_;
		}
	}

	std::vector<Batch> opaque_batches_;

	// Key: z_index, Value: Batches for that z_index.
	std::map<std::int64_t, std::vector<Batch>> transparent_batches_;

	void AddQuad(
		const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
		const std::array<V2_float, 4>& tex_coords, const Texture& t
	) {
		if (t == white_texture_) {
			auto& batch_group = GetBatchGroup(color.w, z_index);
			GetBatch(BatchType::Quad, batch_group).quad_.Get() =
				impl::QuadVertices(vertices, z_index, color, tex_coords, 0.0f);
			return;
		}

		// Textures are always considered as part of the transparent batch groups.
		// In the future one could do a t.HasTransparency() check here to determine batch group.
		auto& batch_group			= GetBatchGroup(0.0f, z_index);
		auto [batch, texture_index] = GetTextureBatch(BatchType::Quad, batch_group, t);
		batch.quad_.Get()			= impl::QuadVertices(
			  vertices, z_index, color, tex_coords, static_cast<float>(texture_index)
		  );
	}

	void AddCircle(
		const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
		float line_width, float fade
	) {
		auto& batch_group = GetBatchGroup(color.w, z_index);
		GetBatch(BatchType::Circle, batch_group).circle_.Get() =
			impl::CircleVertices(vertices, z_index, color, line_width, fade);
	}

	void AddTriangle(
		const V2_float& a, const V2_float& b, const V2_float& c, float z_index,
		const V4_float& color
	) {
		auto& batch_group = GetBatchGroup(color.w, z_index);
		GetBatch(BatchType::Triangle, batch_group).triangle_.Get() =
			impl::TriangleVertices({ a, b, c }, z_index, color);
	}

	void AddLine(const V2_float& p0, const V2_float& p1, float z_index, const V4_float& color) {
		auto& batch_group = GetBatchGroup(color.w, z_index);
		GetBatch(BatchType::Line, batch_group).line_.Get() =
			impl::LineVertices({ p0, p1 }, z_index, color);
	}

	void AddPoint(const V2_float& position, float z_index, const V4_float& color) {
		auto& batch_group = GetBatchGroup(color.w, z_index);
		GetBatch(BatchType::Point, batch_group).point_.Get() =
			impl::PointVertices({ position }, z_index, color);
	}

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

	[[nodiscard]] std::vector<Batch>& GetBatchGroup(float alpha, float z_index) {
		// TODO: Add opaque batches back once you figure out how to do it using depth testing.
		/*
		if (NearlyEqual(alpha, 1.0f)) { // opaque object
			if (opaque_batches_.size() == 0) {
				opaque_batches_.emplace_back(this);
			}
			return opaque_batches_;
		}
		*/
		// transparent object
		auto z_index_key{ static_cast<std::int64_t>(z_index) };
		auto it = transparent_batches_.find(z_index_key);
		if (it != transparent_batches_.end()) {
			return it->second;
		}
		std::vector<Batch> new_batch_group;
		new_batch_group.emplace_back(this);
		auto new_it = transparent_batches_.emplace(z_index_key, std::move(new_batch_group)).first;
		PTGN_ASSERT(new_it->second.size() > 0);
		PTGN_ASSERT(new_it->second.at(0).quad_.GetTextureSlotCapacity() == max_texture_slots_ - 1);
		return new_it->second;
	}

	[[nodiscard]] Batch& GetBatch(BatchType type, std::vector<Batch>& batch_group) {
		PTGN_ASSERT(batch_group.size() > 0);
		auto& latest_batch = batch_group.back();
		if (latest_batch.IsAvailable(type)) {
			return latest_batch;
		}
		return batch_group.emplace_back(this);
	}

	// @return pair<batch reference, texture index>
	[[nodiscard]] std::pair<Batch&, std::size_t> GetTextureBatch(
		BatchType type, std::vector<Batch>& batch_group, const Texture& t
	) {
		PTGN_ASSERT(batch_group.size() > 0);
		PTGN_ASSERT(t.IsValid());
		for (std::size_t i = 0; i < batch_group.size(); i++) {
			auto& batch = batch_group[i];
			if (batch.quad_.IsAvailable()) {
				auto [texture_index, has_available_index] = batch.quad_.GetTextureIndex(t);
				if (has_available_index) {
					return { batch, texture_index };
				}
			}
		}
		auto& new_batch{ batch_group.emplace_back(this) };
		auto [texture_index, has_available_index] = new_batch.quad_.GetTextureIndex(t);
		PTGN_ASSERT(has_available_index);
		PTGN_ASSERT(texture_index == 1);
		return { new_batch, texture_index };
	}
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
	 * @param rotation Degrees to rotate the texture (defaults to 0).
	 * @param rotation_center Fraction of the source_size around which the texture is rotated
	 * (defaults to { 0.5f, 0.5f } which corresponds to the center of the texture).
	 * @param z_index Z-coordinate to draw the texture at (-1.0f to 1.0f).
	 * drawn.
	 */
	void DrawTexture(
		const Texture& texture, const V2_float& destination_position,
		const V2_float& destination_size, const V2_float& source_position = {},
		V2_float source_size = {}, Origin draw_origin = Origin::Center, Flip flip = Flip::None,
		float rotation = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
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

	// Rotation in degrees.
	void DrawRectangleFilled(
		const V2_float& position, const V2_float& size, const Color& color,
		Origin draw_origin = Origin::Center, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRectangleFilled(
		const Rectangle<float>& rectangle, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRectangleHollow(
		const V2_float& position, const V2_float& size, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = 1.0f, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRectangleHollow(
		const Rectangle<float>& rectangle, const Color& color, float line_width = 1.0f,
		float rotation = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
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
		Origin draw_origin = Origin::Center, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRoundedRectangleFilled(
		const RoundedRectangle<float>& rounded_rectangle, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRoundedRectangleHollow(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = 1.0f, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f
	);

	// Rotation in degrees.
	void DrawRoundedRectangleHollow(
		const RoundedRectangle<float>& rounded_rectangle, const Color& color,
		float line_width = 1.0f, float rotation = 0.0f,
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

	void UpdateViewProjection(const M4_float& view_projection);

private:
	friend class CameraManager;
	friend class Game;

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
		Origin origin, float rotation, const V2_float& rotation_center, float z_index
	);

	void DrawRoundedRectangleHollowImpl(
		const V2_float& position, const V2_float& size, float radius, const V4_float& color,
		Origin origin, float line_width, float rotation, const V2_float& rotation_center,
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

	void StartBatch();

	Color clear_color_{ color::White };
	BlendMode blend_mode_{ BlendMode::Blend };
	V2_int viewport_size_;
	impl::RendererData data_;
};

} // namespace ptgn