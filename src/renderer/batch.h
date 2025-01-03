#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "renderer/vertices.h"

// TODO: Currently z_index is not a reliable way of layering drawn objects as it only pertains to a
// single batch type (i.e. quads and circles have their own z_indexs). This can result in unexpected
// behavior if for example the user tries to draw a hollow and filled triangle with varying
// z_indexes as one uses the line batch and one uses the triangle batch. Not sure how to fix this
// while maintaining batching and permitting alpha blending (depth testing requires turning alpha
// blending off). Might be an outdated TODO?
// TODO: If batch size is very small, z_indexes get drawn in 2 calls which completely defeats the
// purpose of it. Might be an outdated TODO?

namespace ptgn::impl {

class RendererData;
class Batch;

struct ShaderVertex {
	ShaderVertex() = default;

	ShaderVertex(
		const VertexArray& vertex_array, const Shader& shader, const Texture& texture,
		BlendMode blend_mode
	) :
		vertex_array{ vertex_array },
		shader{ shader },
		texture{ texture },
		blend_mode{ blend_mode } {}

	VertexArray vertex_array;
	Shader shader;
	Texture texture;
	BlendMode blend_mode{ BlendMode::Blend };
};

class ShaderBatchData {
public:
	[[nodiscard]] bool IsAvailable() const;

	[[nodiscard]] ShaderVertex& Get();

	void Flush(const M4_float& view_projection);

	void Clear();

private:
	friend class Batch;
	friend class RendererData;

	[[nodiscard]] bool IsFlushed() const;

	std::vector<ShaderVertex> data_;
};

template <typename TVertices, std::size_t IndexCount>
class BatchData {
public:
	using vertices = TVertices;

	[[nodiscard]] bool IsAvailable() const;

	[[nodiscard]] TVertices& Get();

	void Flush(const RendererData& renderer);

	void Clear();

private:
	friend class Batch;

	// @return True if buffer was bound during call, false otherwise.
	bool SetupBuffer(const IndexBuffer& index_buffer);

	// @return True if buffer was bound during call, false otherwise.
	bool PrepareBuffer(const RendererData& renderer);

	[[nodiscard]] bool IsFlushed() const;

private:
	VertexArray array_;
	std::vector<TVertices> data_;
};

class TextureBatchData : public BatchData<QuadVertices, 6> {
public:
	TextureBatchData() = delete;
	explicit TextureBatchData(std::size_t max_texture_slots);

	void BindTextures();

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
	Shader
};

class Batch {
public:
	Batch() = delete;
	explicit Batch(std::size_t max_texture_slots);

	[[nodiscard]] bool IsFlushed(BatchType type) const;

	void Flush(const RendererData& renderer, BatchType type, const M4_float& view_projection);

	[[nodiscard]] bool IsAvailable(BatchType type) const;

	void Clear();

	ShaderBatchData shader_;
	TextureBatchData quad_;
	BatchData<CircleVertices, 6> circle_;
	BatchData<TriangleVertices, 3> triangle_;
	BatchData<LineVertices, 2> line_;
	BatchData<PointVertices, 1> point_;
};

using BatchMap = std::map<std::int64_t, std::vector<Batch>>;

struct RenderLayer {
	// Key: z_index, Value: Transparent batches for that z_index.
	BatchMap batch_map;

	// std::vector<Batch> batch_vector; // TODO: Readd opaque batches.

	M4_float view_projection{ 1.0f };
	bool new_view_projection{ false };
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

	ptgn::Shader quad_shader_;
	ptgn::Shader circle_shader_;
	ptgn::Shader color_shader_;

	IndexBuffer quad_ib_;
	IndexBuffer triangle_ib_;
	IndexBuffer line_ib_;
	IndexBuffer point_ib_;
	IndexBuffer shader_ib_; // One set of quad indices.

	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	std::map<std::size_t, RenderLayer> render_layers_;

	void AddShader(
		const ptgn::Shader& shader, const std::array<V2_float, 4>& vertices,
		const ptgn::Texture& texture, BlendMode blend_mode,
		const std::array<V2_float, 4>& tex_coords, float z_index, std::size_t render_layer
	);
	void AddQuad(
		const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
		const std::array<V2_float, 4>& tex_coords, const Texture& t, std::size_t render_layer
	);
	void AddCircle(
		const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
		float line_width, std::size_t render_layer, float fade
	);
	void AddTriangle(
		const V2_float& a, const V2_float& b, const V2_float& c, float z_index,
		const V4_float& color, std::size_t render_layer
	);
	void AddLine(
		const V2_float& p0, const V2_float& p1, float z_index, const V4_float& color,
		std::size_t render_layer
	);
	void AddPoint(
		const V2_float& position, float z_index, const V4_float& color, std::size_t render_layer
	);

	void SetupShaders();

	// Flush a specific render layer. It must exist in render_layers_.
	// @return True if layer was flushed, false if layer batch map was empty.
	bool FlushLayer(RenderLayer& layer, const M4_float& shader_view_projection);

	[[nodiscard]] static std::array<V2_float, 4> GetTextureCoordinates(
		const V2_float& source_position, V2_float source_size, const V2_float& texture_size,
		Flip flip, bool offset_texels = false
	);

	static void FlipTextureCoordinates(
		std::array<V2_float, 4>& texture_coords, Flip flip
	);

	void Shader(
		const ptgn::Shader& shader, const std::array<V2_float, 4>& vertices,
		const ptgn::Texture& texture, BlendMode blend_mode,
		const std::array<V2_float, 4>& tex_coords, float z_index, std::size_t render_layer
	);

	void Texture(
		const std::array<V2_float, 4>& vertices, const Texture& t,
		const std::array<V2_float, 4>& tex_coords, const V4_float& tint_color, float z,
		std::size_t render_layer
	);

	void Point(
		const V2_float& position, const V4_float& color, float radius, float z_index,
		std::size_t render_layer, float fade
	);

	void Line(
		const V2_float& p0, const V2_float& p1, const V4_float& color, float line_width,
		float z_index, std::size_t render_layer
	);

	void Triangle(
		const V2_float& a, const V2_float& b, const V2_float& c, const V4_float& color,
		float line_width, float z_index, std::size_t render_layer
	);

	void Rect(
		const std::array<V2_float, 4>& vertices, const V4_float& color, float line_width,
		float z_index, std::size_t render_layer
	);

	void Polygon(
		const V2_float* vertices, std::size_t vertex_count, const V4_float& color, float line_width,
		float z_index, std::size_t render_layer
	);

	void RoundedRect(
		const V2_float& position, const V2_float& size, float radius, const V4_float& color,
		Origin origin, float line_width, float rotation_radians, const V2_float& rotation_center,
		float z_index, std::size_t render_layer, float fade
	);

	// TODO: Fix ellipse line width being uneven between x and y axes (currently it chooses the
	// smaller radius axis as the relative radius).
	void Ellipse(
		const V2_float& position, const V2_float& radius, const V4_float& color, float line_width,
		float z_index, std::size_t render_layer, float fade
	);

	void Arc(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		bool clockwise, const V4_float& color, float line_width, float z_index,
		std::size_t render_layer, float fade
	);

	// TODO: Fix artefacts in capsule line width at larger radii.
	void Capsule(
		const V2_float& p0, const V2_float& p1, float radius, const V4_float& color,
		float line_width, float z_index, std::size_t render_layer, float fade
	);

	RenderLayer& GetRenderLayer(std::size_t render_layer);

private:
	void FlushType(
		std::vector<Batch>& batches, const ptgn::Shader& shader, BatchType type,
		const M4_float& view_projection, ptgn::Shader& bound_shader
	);

	void FlushBatches(
		std::vector<Batch>& batches, const M4_float& view_projection, ptgn::Shader& bound_shader
	);

	[[nodiscard]] std::vector<Batch>& GetBatchGroup(
		BatchMap& batch_map, float alpha, float z_index
	);

	[[nodiscard]] Batch& GetBatch(BatchType type, std::vector<Batch>& batch_group);

	// @return pair<batch reference, texture index>
	[[nodiscard]] std::pair<Batch&, std::size_t> GetTextureBatch(
		std::vector<Batch>& batch_group, const ptgn::Texture& t
	);
};

} // namespace ptgn::impl
