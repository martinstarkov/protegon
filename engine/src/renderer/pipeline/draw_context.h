#pragma once

#include <array>
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
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"

namespace ptgn {

class Scene;
class SceneContext;
class RenderContext;
class DebugContext;

namespace impl {

class Renderer;

struct TriangleCommand {
	TriangleCommand() = default;

	TriangleCommand(
		const std::array<V2_float, 3>& positions, Color color, std::optional<BlendMode> blend_mode,
		bool floor_positions
	) :
		color{ color },
		positions{ positions },
		blend_mode{ blend_mode },
		floor_positions{ floor_positions } {}

	Color color = color::White;
	std::array<V2_float, 3> positions;
	std::optional<BlendMode> blend_mode;
	bool floor_positions{ true };
};

struct QuadCommand {
	QuadCommand() = default;

	QuadCommand(
		const std::array<V2_float, 4>& positions, Color color, std::optional<BlendMode> blend_mode,
		bool floor_positions
	) :
		color{ color },
		positions{ positions },
		blend_mode{ blend_mode },
		floor_positions{ floor_positions } {}

	Color color = color::White;
	std::array<V2_float, 4> positions;
	std::optional<BlendMode> blend_mode;
	bool floor_positions{ true };
};

struct QuadShapeCommand : public QuadCommand {
	QuadShapeCommand() = default;

	QuadShapeCommand(
		impl::ShaderId shader, const std::array<V2_float, 4>& positions,
		const std::array<float, 4>& user_data, Color color, std::optional<BlendMode> blend_mode,
		bool floor_positions
	) :
		QuadCommand{ positions, color, blend_mode, floor_positions },
		shader{ shader },
		user_data{ user_data } {}

	impl::ShaderId shader;
	std::array<float, 4> user_data;
};

struct TextureCommand {
	TextureCommand() = default;

	TextureCommand(
		impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
		Color tint, const std::array<V2_float, 4>& tex_coords, std::optional<BlendMode> blend_mode,
		bool floor_positions
	) :
		shader{ shader },
		texture{ texture },
		tint{ tint },
		positions{ positions },
		tex_coords{ tex_coords },
		blend_mode{ blend_mode },
		floor_positions{ floor_positions } {}

	impl::ShaderId shader;
	impl::TextureId texture;
	Color tint = color::White;
	std::array<V2_float, 4> positions;
	std::array<V2_float, 4> tex_coords;
	std::optional<BlendMode> blend_mode;
	bool floor_positions{ true };
};

using ManualCommand = std::variant<TextureCommand, QuadCommand, QuadShapeCommand, TriangleCommand>;

struct ManualDrawCommand {
	ManualCommand payload;
	float depth{ 0.0f };
};

using DrawCommandType = std::variant<
	impl::QuadCommand, impl::QuadShapeCommand, std::vector<impl::QuadCommand>,
	std::vector<impl::TriangleCommand>>;

} // namespace impl

class DrawContext {
public:
	void Flush();

	void DrawTexture(
		impl::ShaderId shader, impl::TextureId texture, std::array<V2_float, 4> positions,
		Color tint, float depth, const std::array<V2_float, 4>& tex_coords,
		const std::function<void()>& shader_setup, bool floor_positions
	);

	void DrawTexture(
		impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
		const std::array<V2_float, 4>& tex_coords, bool floor_positions
	);

	void DrawQuad(
		const std::array<V2_float, 4>& positions, Color tint, float depth, bool floor_positions
	);
	void DrawTriangle(
		impl::ShaderId shader, std::array<V2_float, 3> positions, Color tint, float depth,
		bool floor_positions
	);
	void DrawQuad(
		impl::ShaderId shader, std::array<V2_float, 4> positions,
		const std::array<float, 4>& user_data, Color tint, float depth,
		const std::function<void()>& shader_setup, bool floor_positions
	);

	void DrawTexture(
		Texture texture, Transform transform, V2_float size, Origin draw_origin, Color tint,
		float depth, const std::array<V2_float, 4>& texture_coordinates,
		std::optional<BlendMode> blend_mode
	);

	void DrawLines(
		std::span<const V2_float> points, float line_width, Transform transform, Color tint,
		float depth, std::optional<BlendMode> blend_mode, bool connect_last_to_first
	);

	void DrawShape(
		const Shape& shape, Transform transform, Color tint, FillStyle fill_style,
		Origin draw_origin, float depth, std::optional<BlendMode> blend_mode
	);

	impl::TextureId GetWhiteTexture() const;

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

	template <impl::UniformType T>
	void SetUniform(const ShaderVariant& shader, const char* uniform_name, const T& value) {
		renderer_.SetUniform(GetShaderId(shader), uniform_name, value);
	}

private:
	friend class Scene;
	friend class DebugContext;
	friend class RenderContext;

	impl::ShaderId GetShaderId(ShaderVariant shader) const;

	/// @param connect_last_to_first Whether to draw a line connecting the last point back to the
	/// first.
	static std::optional<impl::DrawCommandType> GetDrawCommand(
		std::span<const V2_float> points, float line_width, Transform transform, Color tint,
		std::optional<BlendMode> blend_mode, bool connect_last_to_first, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		const Rect& rect, Transform transform, FillStyle fill_style, Origin draw_origin, Color tint,
		std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		const Triangle& triangle, Transform transform, FillStyle fill_style, Color tint,
		std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		V2_float point, Transform transform, Color tint, std::optional<BlendMode> blend_mode,
		bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId capsule_shader, const Capsule& capsule, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId arc_shader, const Arc& arc, Transform transform, FillStyle fill_style,
		Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId circle_shader, const Ellipse& ellipse, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId circle_shader, const Circle& circle, Transform transform,
		FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		impl::ShaderId rounded_rect_shader, const RoundedRect& rounded_rect, Transform transform,
		FillStyle fill_style, Origin draw_origin, Color tint, std::optional<BlendMode> blend_mode,
		bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		const Polygon& polygon, Transform transform, FillStyle fill_style, Color tint,
		std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		const Line& line, Transform transform, FillStyle fill_style, Color tint,
		std::optional<BlendMode> blend_mode, bool floor_positions
	);

	static std::optional<impl::DrawCommandType> GetDrawCommand(
		const impl::Renderer& renderer, const Shape& shape, Transform transform, Color tint,
		FillStyle fill_style, Origin draw_origin, std::optional<BlendMode> blend_mode
	);

	void Draw(const impl::TextureCommand& draw, float depth);
	void Draw(const impl::QuadCommand& draw, float depth);
	void Draw(const std::vector<impl::QuadCommand>& cmds, float depth);
	void Draw(const impl::QuadShapeCommand& draw, float depth);
	void Draw(const impl::TriangleCommand& draw, float depth);
	void Draw(const std::vector<impl::TriangleCommand>& cmds, float depth);
	void Draw(const impl::ManualCommand& command, float depth);

	DrawContext() = delete;
	explicit DrawContext(impl::Renderer& renderer);

	impl::Renderer& renderer_;
};

} // namespace ptgn