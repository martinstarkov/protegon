#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
#include "renderer/texture.h"

namespace ptgn {

class Shader;

namespace impl {

// Constructing a RenderData object requires the engine to be initialized.
class RenderData {
public:
	// Assumes view_projection_ is updated externally.
	void Flush();

	// @return True if the view projection changed, false otherwise.
	bool SetViewProjection(const Matrix4& view_projection);

	// @return The current view projection matrix for the render data.
	[[nodiscard]] const Matrix4& GetViewProjection() const;

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

private:
	friend struct Batch;

	[[nodiscard]] std::vector<Batch>& GetLayerBatches(
		std::int32_t render_layer, [[maybe_unused]] float alpha
	);

	// @return True if texture is the 1x1 white texture used for solid quads, false otherwise.
	[[nodiscard]] static bool IsBlank(const Texture& texture);

	template <BatchType T, typename VertexType, std::size_t VertexCount>
	void AddPrimitive(
		const std::array<V2_float, VertexCount>& positions, std::int32_t render_layer,
		const V4_float& color, const std::array<V2_float, 4>& tex_coords = {},
		const Texture& texture = {}, float line_width = 0.0f, float fade = 0.0f
	);

	// @return Batch info related to the specific type: std::tuple{ vector of data, shape index
	// count }.
	template <BatchType T>
	static auto GetBufferInfo(Batch& batch) {
		if constexpr (T == BatchType::Quad) {
			return std::make_pair(&batch.quads_, static_cast<std::size_t>(6));
		} else if constexpr (T == BatchType::Triangle) {
			return std::make_pair(&batch.triangles_, static_cast<std::size_t>(3));
		} else if constexpr (T == BatchType::Line) {
			return std::make_pair(&batch.lines_, static_cast<std::size_t>(2));
		} else if constexpr (T == BatchType::Circle) {
			return std::make_pair(&batch.circles_, static_cast<std::size_t>(6));
		} else if constexpr (T == BatchType::Point) {
			return std::make_pair(&batch.points_, static_cast<std::size_t>(1));
		}
	}

	// @return Shader for the specific batch type.
	template <BatchType T>
	static Shader GetShader();

	// @param callback Called before drawing VAO, used for binding textures in case of quads.
	template <BatchType T>
	void FlushType(std::vector<Batch>& batches) const;

	void FlushBatches(std::vector<Batch>& batches);

	// Set to true whenether a primitive using this shader is added.
	// Set to false after flushing the render data.
	bool update_circle_shader_{ false };
	bool update_color_shader_{ false };
	bool update_quad_shader_{ false };

	Matrix4 view_projection_{ 1.0f };
	// Key: Render Layer, Value: Vector of transparent batches for each render layer.
	std::map<std::int32_t, std::vector<Batch>> transparent_layers_;
	// TODO: Readd opaque batches using depth testing.
	// std::vector<Batch> opaque_batches_;
};

} // namespace impl

} // namespace ptgn