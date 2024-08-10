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

enum class Flip {
	// Source: https://wiki.libsdl.org/SDL2/SDL_RendererFlip

	None	   = 0x00000000,
	Horizontal = 0x00000001,
	Vertical   = 0x00000002
};

struct ColorQuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;

	[[nodiscard]] constexpr static std::size_t VertexCount() {
		return 4;
	};

	[[nodiscard]] constexpr static std::size_t IndexCount() {
		return 6;
	}
};

struct QuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
	glsl::float_ tiling_factor;

	[[nodiscard]] constexpr static std::size_t VertexCount() {
		return 4;
	};

	[[nodiscard]] constexpr static std::size_t IndexCount() {
		return 6;
	}
};

struct CircleVertex {
	glsl::vec3 position;
	glsl::vec3 local_position;
	glsl::vec4 color;
	glsl::float_ thickness;
	glsl::float_ fade;

	[[nodiscard]] constexpr static std::size_t VertexCount() {
		return 4;
	};

	[[nodiscard]] constexpr static std::size_t IndexCount() {
		return 6;
	}
};

struct LineVertex {
	glsl::vec3 position;
	glsl::vec4 color;

	[[nodiscard]] constexpr static std::size_t VertexCount() {
		return 2;
	};

	[[nodiscard]] constexpr static std::size_t IndexCount() {
		return 2;
	}
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

template <typename TVertex>
class BatchData {
public:
	VertexArray array_;
	VertexBuffer buffer_;
	Shader shader_;
	std::size_t index_count_{ 0 };
	std::vector<TVertex> buffer_base_;
	std::int32_t buffer_index_{ -1 };

	constexpr static const std::size_t batch_count_	 = 20000;
	constexpr static const std::size_t max_vertices_ = batch_count_ * TVertex::VertexCount();
	constexpr static const std::size_t max_indices_	 = batch_count_ * TVertex::IndexCount();

	// index_multiplier allows for drawing more complicated shapes from primitives, for instance a
	// hollow rectangle drawn from 4 lines would have an index_multiplier of 4.
	// @return True if batch was advanced, false if it was not.
	bool AdvanceBatch(std::size_t index_multiplier = 1) {
		if (index_count_ + TVertex::IndexCount() * index_multiplier >= max_indices_) {
			NextBatch();
			return true;
		}
		return false;
	}

	void NextBatch() {
		Draw();
		Reset();
	}

	bool IsFlushed() const {
		return index_count_ == 0;
	}

	constexpr static V2_float GetDrawOffset(const V2_float& size, Origin draw_origin) {
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

	constexpr static void OffsetVertices(
		std::array<V2_float, QuadVertex::VertexCount()>& vertices, const V2_float& size,
		Origin draw_origin
	) {
		auto draw_offset = GetDrawOffset(size, draw_origin);

		// Offset each vertex by based on draw origin.
		if (!draw_offset.IsZero()) {
			for (auto& v : vertices) {
				v -= draw_offset;
			}
		}
	}

	constexpr static void FlipVertices(
		std::array<V2_float, QuadVertex::VertexCount()>& vertices, Flip flip
	) {
		switch (flip) {
			case Flip::None:	   break;
			case Flip::Horizontal: {
				std::swap(vertices[0].x, vertices[1].x);
				std::swap(vertices[2].x, vertices[3].x);
				break;
			}
			case Flip::Vertical: {
				std::swap(vertices[0].y, vertices[3].y);
				std::swap(vertices[1].y, vertices[2].y);
				break;
			}
			default: PTGN_ERROR("Failed to identify texture flip");
		}
	}

	constexpr static void RotateVertices(
		std::array<V2_float, QuadVertex::VertexCount()>& vertices, const V2_float& position,
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

		auto rotated = [=](const V2_float& coordinate) -> V2_float {
			return { c * coordinate.x - s * coordinate.y - half.x - rot.x,
					 s * coordinate.x + c * coordinate.y - half.y - rot.y };
		};

		vertices[0] = position + rotated(s0);
		vertices[1] = position + rotated(s1);
		vertices[2] = position + rotated(s2);
		vertices[3] = position + rotated(s3);
	}

	constexpr static std::array<V2_float, QuadVertex::VertexCount()> GetQuadVertices(
		const V2_float& position, const V2_float& size, Flip flip, Origin draw_origin,
		float rotation, const V2_float& rotation_center
	) {
		std::array<V2_float, QuadVertex::VertexCount()> vertices;

		RotateVertices(vertices, position, size, rotation, rotation_center);
		OffsetVertices(vertices, size, draw_origin);
		FlipVertices(vertices, flip);

		return vertices;
	}

	template <typename... TLayouts>
	void Init(std::size_t vertex_count, PrimitiveMode mode, const IndexBuffer& index_buffer) {
		// TODO: Consider resizing buffer dynamically as demand grows?
		buffer_base_.resize(vertex_count);
		buffer_ = VertexBuffer(buffer_base_, BufferLayout<TLayouts...>{}, BufferUsage::DynamicDraw);

		array_ = { mode, buffer_, index_buffer };
	}

	void SetupShader(
		const path& vertex, const path& fragment, const std::vector<std::int32_t>& samplers
	) {
		shader_ = Shader(vertex, fragment);
		shader_.Bind();
		shader_.SetUniform("u_Textures", samplers.data(), samplers.size());
	}

	void SetupBatch() {
		PTGN_ASSERT(buffer_index_ != -1);
		std::uint32_t data_size = static_cast<std::uint32_t>(buffer_index_) * sizeof(TVertex);
		// Sort by z-index before sending to GPU.
		SortZPositions();
		buffer_.SetSubData(buffer_base_.data(), data_size);
		shader_.Bind();
	}

	void Draw() {
		PTGN_ASSERT(index_count_ != 0);
		SetupBatch();
		GLRenderer::DrawElements(array_, index_count_);
	}

	void SortZPositions() {
		std::sort(
			buffer_base_.begin(), buffer_base_.begin() + index_count_ / TVertex::IndexCount(),
			[](const TVertex& a, const TVertex& b) { return a.position[2] > b.position[2]; }
		);
	}

	void AddQuad(
		const V3_float& position, const V2_float& size, const V4_float& color, Flip flip,
		const std::array<V2_float, 4>& tex_coords, float texture_index, float tiling_factor,
		Origin origin, float rotation, const V2_float& rotation_center
	);

	void AddCircle(
		const V3_float& position, const V2_float& size, const V4_float& color, float thickness,
		float fade
	);

	void AddLine(
		const V2_float& p0, float z_index0, const V2_float& p1, float z_index1,
		const V4_float& color
	);

	void IncrementBuffer() {
		PTGN_ASSERT(buffer_index_ != -1);
		++buffer_index_;
	}

	void Reset() {
		index_count_  = 0;
		buffer_index_ = 0;
	}
};

class RendererData {
public:
	RendererData();
	~RendererData() = default;

	std::uint32_t max_texture_slots_{ 0 };

	M4_float view_projection_;

	Texture white_texture_;

	std::vector<Texture> texture_slots_;
	std::uint32_t texture_slot_index_{ 1 }; // 0 reserved for white texture

	BatchData<QuadVertex> quad_;
	BatchData<CircleVertex> circle_;
	BatchData<LineVertex> line_;
	// BatchData<TextVertex> text_;

	float line_width_{ 2.0f };

	// Texture font_atlas_texture_;

	void SetupBuffers();
	void SetupTextureSlots();
	void SetupShaders();

	void Flush();

	void BindTextures() const;

	[[nodiscard]] static std::array<V2_float, QuadVertex::VertexCount()> GetTextureCoordinates(
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

	template <typename T, std::size_t I>
	[[nodiscard]] constexpr static std::vector<IndexBuffer::IndexType> GetQuadIndices() {
		std::vector<IndexBuffer::IndexType> indices;
		indices.resize(I);

		std::uint32_t offset{ 0 };
		for (std::size_t i{ 0 }; i < indices.size(); i += T::IndexCount()) {
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += static_cast<std::uint32_t>(T::VertexCount());
		}

		return indices;
	}

	template <typename T, std::size_t I>
	[[nodiscard]] constexpr static std::vector<IndexBuffer::IndexType> GetLineIndices() {
		std::vector<IndexBuffer::IndexType> indices;
		indices.resize(I);

		std::uint32_t offset{ 0 };
		for (std::size_t i{ 0 }; i < indices.size(); i += T::IndexCount()) {
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;

			offset += static_cast<std::uint32_t>(T::VertexCount());
		}

		return indices;
	}

	struct Stats {
		std::int64_t quad_count{ 0 };
		std::int64_t circle_count{ 0 };
		std::int64_t line_count{ 0 };
		std::int64_t draw_calls{ 0 };

		void Reset() {
			quad_count	 = 0;
			circle_count = 0;
			line_count	 = 0;
			draw_calls	 = 0;
		};

		void Print() {
			PTGN_INFO(
				"Draw Calls: ", draw_calls, ", Quads: ", quad_count, ", Circles: ", circle_count,
				", Lines: ", line_count
			);
		}
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