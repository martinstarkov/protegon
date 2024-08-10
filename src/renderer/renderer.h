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

#define PTGN_OPENGL_MAJOR_VERSION 3
#define PTGN_OPENGL_MINOR_VERSION 3

namespace ptgn {

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
	glsl::float_ thickness;
	glsl::float_ fade;
};

struct LineVertex {
	glsl::vec3 position;
	glsl::vec4 color;
};

enum class Origin {
	Center,
	TopLeft,
	TopRight,
	BottomRight,
	BottomLeft,
};

namespace impl {

class RendererData;

class QuadData {
public:
	constexpr static std::size_t vertex_count{ 4 };
	constexpr static std::size_t index_count{ 6 };

	[[nodiscard]] float GetZIndex() const;

	void Add(
		const std::array<V2_float, vertex_count> positions, float z_index, const V4_float& color,
		const std::array<V2_float, 4>& tex_coords, float texture_index, float tiling_factor
	);

private:
	std::array<QuadVertex, vertex_count> vertices_;
};

class CircleData {
public:
	constexpr static const std::size_t vertex_count{ 4 };
	constexpr static const std::size_t index_count{ 6 };

	[[nodiscard]] float GetZIndex() const;

	void Add(
		const std::array<V2_float, vertex_count> positions, float z_index, const V4_float& color,
		float thickness, float fade
	);

private:
	std::array<CircleVertex, vertex_count> vertices_;
};

class LineData {
public:
	constexpr static std::size_t vertex_count{ 2 };
	constexpr static std::size_t index_count{ 2 };

	[[nodiscard]] float GetZIndex() const;

	void Add(const V3_float& p0, const V3_float& p1, const V4_float& color);

private:
	std::array<LineVertex, vertex_count> vertices_;
};

[[nodiscard]] V2_float GetDrawOffset(const V2_float& size, Origin draw_origin);

[[nodiscard]] void OffsetVertices(
	std::array<V2_float, QuadData::vertex_count>& vertices, const V2_float& size, Origin draw_origin
);

[[nodiscard]] void FlipTextureCoordinates(
	std::array<V2_float, QuadData::vertex_count>& texture_coords, Flip flip
);

[[nodiscard]] void RotateVertices(
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

	constexpr static const std::size_t batch_count_	 = 20000;
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

	void SetupShader(
		const path& vertex, const path& fragment, const std::vector<std::int32_t>& samplers
	) {
		shader_ = Shader(vertex, fragment);
		shader_.Bind();
		shader_.SetUniform("u_Textures", samplers.data(), samplers.size());
	}

	void Draw() {
		PTGN_ASSERT(index_ != -1);
		// Sort by z-index before sending to GPU.
		std::sort(batch_.begin(), batch_.begin() + index_ + 1, [](const T& a, const T& b) {
			return a.GetZIndex() < b.GetZIndex();
		});
		buffer_.SetSubData(batch_.data(), static_cast<std::uint32_t>(index_ + 1) * sizeof(T));
		shader_.Bind();
		GLRenderer::DrawElements(array_, (index_ + 1) * T::index_count);
		index_ = -1;
	}
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
	BatchData<LineData> line_;

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
		std::int64_t line_count{ 0 };
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

	// Rotation in degrees.
	void DrawRectangleFilled(
		const V2_float& position, const V2_float& size, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		Origin origin = Origin::Center
	);

	// Rotation in degrees.
	void DrawRectangleHollow(
		const V2_float& position, const V2_float& size, const Color& color, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		Origin origin = Origin::Center
	);

	/*
	 * @param source_position Top left pixel to start drawing texture from within the texture
	 * (defaults to { 0, 0 }).
	 * @param source_size Number of pixels of the texture to draw (defaults to {} which corresponds
	 * to the remaining texture size to the bottom right of source_position).
	 * @param rotation Number of degrees to rotate the texture (defaults to 0).
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
		Flip flip = Flip::None, float z_index = 0.0f, Origin draw_origin = Origin::Center,
		float tiling_factor = 1.0f, const Color& tint_color = color::White
	);

	void DrawCircleSolid(
		const V2_float& position, float radius, const Color& color, float z_index = 0.0f,
		float thickness = 1.0f, float fade = 0.005f
	);

	void DrawLine(const V3_float& p0, const V3_float& p1, const Color& color);
	void DrawLine(const V2_float& p0, const V2_float& p1, const Color& color);

	void SetBlendMode(BlendMode mode);
	BlendMode GetBlendMode() const;

	void SetClearColor(const Color& color);
	Color GetClearColor() const;

	// @param width Line width in pixels.
	void SetLineWidth(float width);
	// @return Line width in pixels.
	float GetLineWidth() const;

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