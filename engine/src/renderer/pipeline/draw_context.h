#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/render_packet.h"
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"

namespace ptgn {

class Scene;
class SceneContext;
class RenderContext;
class DebugContext;

namespace impl {

class Renderer;
class SceneGraphBuilder;

} // namespace impl

struct DrawOptions {
	Origin origin{ Origin::Center };
	Color tint{ color::White };
	float depth{ 0.0f };
	std::optional<BlendMode> blend_mode;
	std::optional<std::array<V2_float, 4>> tex_coords;
	// TODO: Add entity id.
};

struct ShapeDrawOptions {
	Color color{ color::White };
	FillStyle fill_style;
	Origin origin{ Origin::Center };
	float depth{ 0.0f };
	std::optional<BlendMode> blend_mode;
	// TODO: Add entity id.
};

class DrawContext {
public:
	void DrawTexture(
		impl::TextureId texture, Transform transform, V2_float size, DrawOptions options = {}
	);
	void DrawShape(const Shape& shape, Transform transform, ShapeDrawOptions options = {});

	// TODO: Fix.
	// void DrawText(const impl::TextLayout& layout, Depth depth);

	impl::SceneGraphBuilder& GetGraphBuilder();
	const impl::SceneGraphBuilder& GetGraphBuilder() const;

	template <typename F>
	void WithState(impl::RenderState delta, F&& f) {
		auto previous  = pending_state_;
		pending_state_ = ApplyDelta(pending_state_, delta);
		std::forward<F>(f)();
		pending_state_ = previous;
	}

	// TODO: Fix.
	/*
	void Flush();

	void DrawTriangle(
		impl::ShaderId shader, const std::array<V2_float, 3>& positions, float depth, Color tint,
		int entity_id
	);

	void DrawQuad(
		impl::ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
		int entity_id
	);

	void DrawShape(
		impl::ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const std::array<float, 4>& shape_data,
		int entity_id
	);

	void DrawShader(
		impl::ShaderId shader, std::array<V2_float, 4> positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const std::function<void()>& shader_setup,
		int entity_id
	);

	void DrawTexture(
		impl::ShaderId shader, impl::TextureId texture, std::array<V2_float, 4> positions,
		float depth, Color tint, const std::array<V2_float, 4>& tex_coords,
		const std::function<void()>& shader_setup, int entity_id
	);

	void DrawTexture(
		impl::TextureId texture, std::array<V2_float, 4> positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, int entity_id
	);

	void DrawLines(
		std::span<const V2_float> points, float line_width, Transform transform, Color tint,
		float depth, std::optional<BlendMode> blend_mode, bool connect_last_to_first
	);

	void DrawTexture(
		impl::TextureId texture, Transform transform, float depth, V2_float size,
		Origin draw_origin, Color tint, const std::array<V2_float, 4>& tex_coords,
		std::optional<BlendMode> blend_mode, int entity_id
	);

	void DrawShape(
		const Shape& shape, Transform transform, float depth, Color tint, FillStyle fill_style,
		Origin draw_origin, std::optional<BlendMode> blend_mode, int entity_id
	);

	template <
		impl::VertexType TVertex,
		typename TAccessor = impl::Renderer::DefaultTextureIndexAccessor<TVertex>>
	void DrawTexturedQuads(
		std::string_view pipeline_name, impl::ShaderId shader, std::span<TVertex> vertices,
		std::span<const std::uint32_t> local_indices,
		std::span<const impl::TextureId> textures	  = {},
		std::optional<std::size_t> batch_state_hash	  = std::nullopt,
		const impl::Renderer::BatchSetup& batch_setup = {}, TAccessor get_tex_index = {}
	) {
		renderer_.DrawTexturedQuads(
			pipeline_name, shader, vertices, local_indices, textures, batch_state_hash, batch_setup,
			get_tex_index
		);
	}

	impl::ShaderId GetShader(std::string_view name) const;

	void BindScreenTarget();

	void SetViewport(Viewport viewport);
	void SetViewProjection(const Matrix4& view_projection);
	void SetBlend(bool enabled);
	void SetBlendMode(BlendMode mode);
	void SetDepthTesting(bool enabled);
	void SetDepthMask(const DepthMaskState& mask);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetShader(const Shader& shader);
	void SetShader(impl::ShaderId shader);
	void SetScissor(const ScissorState& scissor);
	void SetColorMask(const ColorMaskState& color_mask);

	[[nodiscard]] impl::RenderPass BeginPass(impl::RenderTargetId scene_render_target);

	using ShaderVariant = std::variant<Shader, impl::ShaderId, std::string_view>;

	template <typename T>
	void SetUniform(const ShaderVariant& shader, const char* uniform_name, const T& value) {
		renderer_.SetUniform(GetShaderId(shader), uniform_name, value);
	}

	*/
private:
	friend class impl::SceneGraphBuilder;

	DrawContext(impl::Renderer& renderer, impl::SceneGraphBuilder& graph_builder);

	void SubmitRenderPacket(impl::RenderPacket packet);

	impl::RenderState pending_state_;
	/*
	friend class Scene;
	friend class DebugContext;
	friend class RenderContext;

	explicit DrawContext(impl::Renderer& renderer);

	impl::ShaderId GetShaderId(ShaderVariant shader) const;

	/// @param connect_last_to_first Whether to draw a line connecting the last point back to the
	/// first.
	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId quad_shader, std::span<const V2_float> points, float line_width,
		Transform transform, Color tint, std::optional<BlendMode> blend_mode,
		bool connect_last_to_first, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId quad_shader, const Rect& rect, Transform transform, FillStyle fill_style,
		Origin draw_origin, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId triangle_shader, const Triangle& triangle, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId point_shader, V2_float point, Transform transform, Color tint,
		std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId capsule_shader, const Capsule& capsule, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId arc_shader, const Arc& arc, Transform transform, FillStyle fill_style,
		Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId ellipse_shader, const Ellipse& ellipse, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId circle_shader, const Circle& circle, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId rect_shader, impl::ShaderId rounded_rect_shader,
		const RoundedRect& rounded_rect, Transform transform, FillStyle fill_style,
		Origin draw_origin, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId polygon_shader, const Polygon& polygon, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId quad_shader, const Line& line, Transform transform, FillStyle fill_style,
		Color tint, std::optional<BlendMode> blend_mode, int entity_id
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		const impl::Renderer& renderer, const Shape& shape, Transform transform, Color tint,
		FillStyle fill_style, Origin draw_origin, std::optional<BlendMode> blend_mode, int entity_id
	);

	void Draw(const impl::TextureCommand& draw, float depth);
	void Draw(const impl::QuadCommand& draw, float depth);
	void Draw(const std::vector<impl::QuadCommand>& cmds, float depth);
	void Draw(const impl::ShapeCommand& draw, float depth);
	void Draw(const impl::TriangleCommand& draw, float depth);
	void Draw(const std::vector<impl::TriangleCommand>& cmds, float depth);
	void Draw(const impl::ManualCommand& command, float depth);

	DrawContext() = delete;

	impl::Renderer& renderer_;
	*/

	impl::Renderer& renderer_;
	impl::SceneGraphBuilder& graph_builder_;
};

} // namespace ptgn