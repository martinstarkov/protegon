#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "renderer/vertices.h"
#include "renderer/texture.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/vertex_array.h"
#include "utility/utility.h"

namespace ptgn {

struct Polygon;
struct Capsule;
struct Arc;
struct RoundedRect;
struct Triangle;
struct Line;
struct Ellipse;

namespace impl {

enum class BatchType {
	Quad,
	Circle,
	Triangle,
	Line,
	Point
};

struct Batch {
	Batch() = default;

	Batch(const Texture& texture);

	std::vector<Texture> textures_;
	std::vector<std::array<QuadVertex, 4>> quads_;
	std::vector<std::array<CircleVertex, 4>> circles_;
	std::vector<std::array<ColorVertex, 3>> triangles_;
	std::vector<std::array<ColorVertex, 2>> lines_;
	std::vector<std::array<ColorVertex, 1>> points_;

	void BindTextures() const;

	// Add a texture to the batch and returns its texture index.
	// @return The texture index to which the texture was added. If the batch texture slots are
	// full, an index of 0.0f is returned.
	[[nodiscard]] float GetAvailableTextureIndex(const Texture& texture);
};

// Constructing a RenderData object requires the engine to be initialized.
class RenderData {
public:
	RenderData();

	// Assumes view_projection_ is updated externally.
	void Flush();

	void SetViewProjection(const M4_float& view_projection);

	void AddTexture(
		const std::array<V2_float, 4>& vertices, const Texture& texture,
		const std::array<V2_float, 4>& tex_coords, const V4_float& tint_color,
		std::int32_t render_layer
	);

	void AddEllipse(
		const Ellipse& ellipse, const V4_float& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddLine(
		const Line& line, const V4_float& color, float line_width, std::int32_t render_layer
	);

	void AddPoint(
		const V2_float& point, const V4_float& color, float radius, float fade,
		std::int32_t render_layer
	);

	void AddTriangle(
		const Triangle& triangle, const V4_float& color, float line_width, std::int32_t render_layer
	);

	void AddRect(
		const std::array<V2_float, 4>& vertices, const V4_float& color, float line_width,
		std::int32_t render_layer
	);

	void AddRoundedRect(
		const RoundedRect& rrect, const V4_float& color, float line_width,
		const V2_float& rotation_center, float fade, std::int32_t render_layer
	);

	void AddArc(
		const Arc& arc, bool clockwise, const V4_float& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddCapsule(
		const Capsule& capsule, const V4_float& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddPolygon(
		const Polygon& polygon, const V4_float& color, float line_width, std::int32_t render_layer
	);

private:
	friend struct Batch;

	void AddPrimitiveQuad(
		const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
		const std::array<V2_float, 4>& tex_coords, const Texture& texture
	);

	void AddPrimitiveCircle(
		const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
		float line_width, float fade
	);

	void AddPrimitiveTriangle(
		const std::array<V2_float, 3>& positions, std::int32_t render_layer, const V4_float& color
	);

	void AddPrimitiveLine(
		const std::array<V2_float, 2>& positions, std::int32_t render_layer, const V4_float& color
	);

	void AddPrimitivePoint(
		const std::array<V2_float, 1>& positions, std::int32_t render_layer, const V4_float& color
	);

	[[nodiscard]] std::vector<Batch>& GetLayerBatches(std::int32_t render_layer, [[maybe_unused]] float alpha);

	// @return True if texture is the 1x1 white texture used for solid quads, false otherwise.
	[[nodiscard]] static bool IsBlank(const Texture& texture);

	// @return pair.first is the vector to which the primitive can be added, pair.second is the
	// texture index.
	template <BatchType T, typename VertexType, std::size_t VertexCount>
	std::pair<std::vector<std::array<VertexType, VertexCount>>&, float> GetAvailableBatch(
		const Texture& texture, std::int32_t render_layer, float alpha
	) {
		float texture_index{ 0.0f };

		constexpr bool is_quad{ std::is_same_v<VertexType, QuadVertex> };

		// Textures are currently always considered part of the transparent batches.
		// TODO: Check texture format (e.g. RGB888) to separate it into the opaque batches.
		auto& batches{ GetLayerBatches(render_layer, is_quad ? 0.0f : alpha) };
		PTGN_ASSERT(!batches.empty());

		std::vector<std::array<VertexType, VertexCount>>* data{ nullptr };

		if (is_quad && !IsBlank(texture)) {
			PTGN_ASSERT(texture.IsValid());

			for (auto& batch : batches) {
				if (batch.quads_.size() == batch_capacity_) {
					continue;
				}
				if (auto index{ batch.GetAvailableTextureIndex(texture) }; index != 0.0f) {
					data		  = &batch.quads_;
					texture_index = index;
					break;
				}
			}
			// An available/existing texture index was not found, therefore add a new batch.
			if (texture_index == 0.0f) {
				texture_index = 1.0f;
				data		  = &batches.emplace_back(texture).quads_;
			}
		} else {
			data = std::get<0>(GetBufferInfo<T>(batches.back()));
			if (data->size() == batch_capacity_) {
				data = std::get<0>(GetBufferInfo<T>(batches.emplace_back()));
			}
		}

		PTGN_ASSERT(data != nullptr);
		PTGN_ASSERT(data->size() + 1 <= batch_capacity_);

		return { *data, texture_index };
	}

	template <BatchType T, typename VertexType, std::size_t VertexCount>
	void AddPrimitive(
		const std::array<V2_float, VertexCount>& positions, std::int32_t render_layer,
		const V4_float& color, const std::array<V2_float, 4>& tex_coords = {},
		const Texture& texture = {}, float line_width = 0.0f, float fade = 0.0f
	) {
		PTGN_ASSERT(color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f && color.w >= 0.0f);
		PTGN_ASSERT(color.x <= 1.0f && color.y <= 1.0f && color.z <= 1.0f && color.w <= 1.0f);

		auto& [data, texture_index] =
			GetAvailableBatch<T, VertexType, VertexCount>(texture, render_layer, color.w);

		// Used for circle vertices.
		constexpr std::array<V2_float, 4> local{ V2_float{ -1.0f, -1.0f }, V2_float{ 1.0f, -1.0f },
												 V2_float{ 1.0f, 1.0f }, V2_float{ -1.0f, 1.0f } };

		std::array<VertexType, VertexCount> vertices;

		for (std::size_t i{ 0 }; i < VertexCount; i++) {
			vertices[i].position = { positions[i].x, positions[i].y,
									 static_cast<float>(render_layer) };
			vertices[i].color	 = { color.x, color.y, color.z, color.w };
			if constexpr (std::is_same_v<VertexType, QuadVertex>) {
				vertices[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
				vertices[i].tex_index = { texture_index };
			} else if constexpr (std::is_same_v<VertexType, CircleVertex>) {
				// local z coordinate provided for memory alignment.
				vertices[i].local_position = { local[i].x, local[i].y, 0.0f };
				vertices[i].line_width	   = { line_width };
				vertices[i].fade		   = { fade };
			}
		}

		data->emplace_back(vertices);
	}

	// @return Batch info related to the specific type: std::tuple{ vector of data, shape index
	// count }.
	template <BatchType T>
	static auto GetBufferInfo(Batch& batch) {
		if constexpr (T == BatchType::Quad) {
			return std::make_tuple(batch.quads_, 6);
		} else if constexpr (T == BatchType::Triangle) {
			return std::make_tuple(batch.triangles_, 3);
		} else if constexpr (T == BatchType::Line) {
			return std::make_tuple(batch.lines_, 2);
		} else if constexpr (T == BatchType::Circle) {
			return std::make_tuple(batch.circles_, 6);
		} else if constexpr (T == BatchType::Point) {
			return std::make_tuple(batch.points_, 1);
		}
	}

	template <BatchType T>
	const VertexArray& GetVertexArray() const {
		if constexpr (T == BatchType::Quad) {
			return quad_vao_;
		} else if constexpr (T == BatchType::Triangle) {
			return triangle_vao_;
		} else if constexpr (T == BatchType::Line) {
			return line_vao_;
		} else if constexpr (T == BatchType::Circle) {
			return circle_vao_;
		} else if constexpr (T == BatchType::Point) {
			return point_vao_;
		}
	}

	// @param callback Called before drawing VAO, used for binding textures in case of quads.
	template <BatchType T>
	void FlushType(const std::vector<Batch>& batches) {
		auto vao{ GetVertexArray<T>() };
		vao.Bind();
		for (auto& batch : batches) {
			auto [data, index_count] = GetBufferInfo<T>(batch);
			if (data.empty()) {
				continue;
			}
			if constexpr (T == BatchType::Quad) {
				batch.BindTextures();
			}
			vao.GetVertexBuffer().SetSubData(
				data.data(), static_cast<std::uint32_t>(SizeofVector(data)), false
			);
			vao.Draw(data.size() * index_count, false);
			// data.clear(); // Not needed since transparent_layers_ is cleared every frame.
		}
	}

	void FlushBatches(const std::vector<Batch>& batches);

	// Maximum number of primitive types before a second batch is generated.
	// The higher the number, the less draw calls but more RAM is used.
	std::size_t batch_capacity_{ 0 };

	bool refresh_view_projection_{ true };
	M4_float view_projection_{ 1.0f };
	// Key: Render Layer, Value: Vector of transparent batches for each render layer.
	std::map<std::int32_t, std::vector<Batch>> transparent_layers_;
	// TODO: Readd opaque batches using depth testing.
	// std::vector<Batch> opaque_batches_;

	VertexArray quad_vao_;
	VertexArray circle_vao_;
	VertexArray triangle_vao_;
	VertexArray line_vao_;
	VertexArray point_vao_;
};

} // namespace impl

} // namespace ptgn