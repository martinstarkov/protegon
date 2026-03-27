#pragma once

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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
#include "core/util/concepts.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/scaling_mode.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/viewport.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"

namespace ptgn {

class Scene;
class SceneContext;
class Renderer;
class RenderContext;
class DebugContext;

namespace impl {

struct TriangleCommand {
	TriangleCommand() = default;

	TriangleCommand(
		const std::array<V2_float, 3>& positions, Color color, std::optional<BlendMode> blend_mode
	) :
		color{ color }, positions{ positions }, blend_mode{ blend_mode } {}

	Color color = color::White;
	std::array<V2_float, 3> positions;
	std::optional<BlendMode> blend_mode;
};

struct QuadCommand {
	QuadCommand() = default;

	QuadCommand(
		const std::array<V2_float, 4>& positions, Color color, std::optional<BlendMode> blend_mode
	) :
		color{ color }, positions{ positions }, blend_mode{ blend_mode } {}

	Color color = color::White;
	std::array<V2_float, 4> positions;
	std::optional<BlendMode> blend_mode;
};

struct QuadShapeCommand : public QuadCommand {
	QuadShapeCommand() = default;

	QuadShapeCommand(
		impl::ShaderId shader, const std::array<V2_float, 4>& positions,
		const std::array<float, 4>& user_data, Color color, std::optional<BlendMode> blend_mode
	) :
		QuadCommand{ positions, color, blend_mode }, shader{ shader }, user_data{ user_data } {}

	impl::ShaderId shader;
	std::array<float, 4> user_data;
};

struct TextureCommand {
	TextureCommand() = default;

	TextureCommand(
		impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
		Color tint, const std::array<V2_float, 4>& tex_coords, std::optional<BlendMode> blend_mode
	) :
		shader{ shader },
		texture{ texture },
		tint{ tint },
		positions{ positions },
		tex_coords{ tex_coords },
		blend_mode{ blend_mode } {}

	impl::ShaderId shader;
	impl::TextureId texture;
	Color tint = color::White;
	std::array<V2_float, 4> positions;
	std::array<V2_float, 4> tex_coords;
	std::optional<BlendMode> blend_mode;
};

using ManualCommand = std::variant<TextureCommand, QuadCommand, QuadShapeCommand, TriangleCommand>;

struct ManualDrawCommand {
	ManualCommand payload;
	float depth{ 0.0f };
};

struct DrawCommand {
	std::variant<Entity, ManualCommand> payload;
	float depth{ 0.0f };
};

} // namespace impl

class DrawContext {
public:
	void Flush();

	void DrawTexture(
		impl::ShaderId shader, impl::TextureId texture, std::array<V2_float, 4> positions,
		Color tint, float depth, const std::array<V2_float, 4>& tex_coords,
		const std::function<void()>& shader_setup = {}
	);

	void DrawTexture(
		impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
		const std::array<V2_float, 4>& tex_coords
	);

	void DrawQuad(const std::array<V2_float, 4>& positions, Color tint, float depth);
	void DrawTriangle(
		impl::ShaderId shader, std::array<V2_float, 3> positions, Color tint, float depth
	);
	void DrawQuad(
		impl::ShaderId shader, std::array<V2_float, 4> positions,
		const std::array<float, 4>& user_data, Color tint, float depth,
		const std::function<void()>& shader_setup = {}
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
	void SetBlend(BlendMode mode, bool enabled = true);
	void SetDepth(const DepthState& depth);
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

private:
	friend class Renderer;
	friend class Scene;
	friend class DebugContext;
	friend class RenderContext;

	impl::ShaderId GetShaderId(ShaderVariant shader) const;

	static std::optional<std::variant<
		impl::QuadCommand, impl::QuadShapeCommand, std::vector<impl::QuadCommand>,
		std::vector<impl::TriangleCommand>>>
	GetShapeDrawCommand(
		Renderer& renderer, const Shape& shape, Transform transform, Color tint,
		FillStyle fill_style, Origin draw_origin, std::optional<BlendMode> blend_mode
	);

	/// @param connect_last_to_first Whether to draw a line connecting the last point back to the
	/// first.
	static std::vector<impl::QuadCommand> GetLineDrawCommands(
		std::span<const V2_float> points, float line_width, Transform transform, Color tint,
		std::optional<BlendMode> blend_mode, bool connect_last_to_first
	);

	void Draw(const impl::TextureCommand& draw, float depth);
	void Draw(const impl::QuadCommand& draw, float depth);
	void Draw(const std::vector<impl::QuadCommand>& cmds, float depth);
	void Draw(const impl::QuadShapeCommand& draw, float depth);
	void Draw(const impl::TriangleCommand& draw, float depth);
	void Draw(const std::vector<impl::TriangleCommand>& cmds, float depth);
	void Draw(const impl::ManualCommand& command, float depth);

	DrawContext() = delete;
	explicit DrawContext(Renderer& renderer);

	Renderer& renderer_;
};

class RenderContext {
public:
	/// @param game_size Setting to {} will use dynamic window size.
	void SetGameSize(
		std::optional<V2_int> game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	Viewport GetDisplayViewport() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The game size of the renderer. Returns window size if unset.
	V2_int GetGameSize() const;

	/// @return The game size scaling mode.
	ScalingMode GetScalingMode() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	void DrawTexture(
		TextureOrKey texture, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = {},
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = {},
		const std::optional<Camera>& camera								  = {}
	);

	void DrawTexture(
		TextureOrKey texture, Shader shader, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = {},
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = {},
		const std::optional<Camera>& camera								  = {}
	);

	/// @param size If size is {}, uses the entire game size.
	/// @param user_data Optional array of 4 floats that can be used to pass per vertex data to the
	/// shader.
	void DrawShader(
		Shader shader, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {},
		const std::optional<std::array<float, 4>>& user_data = {}
	);

	void DrawLines(
		const std::vector<V2_float>& points, Color color, float line_width = kMinLineWidth,
		bool connect_last_to_first = false, std::optional<Transform> transform = {},
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawShape(
		const Shape& shape, Transform transform, Color color, FillStyle fill_style,
		Origin draw_origin = Origin::Center, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawText(
		std::string_view text_content, Transform transform, Color text_color,
		FontSize font_size = {}, FontOrKey font = {}, const TextProperties& properties = {},
		Origin draw_origin = Origin::Center, std::optional<V2_float> text_size = {},
		bool hd_text = true, Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawRect(
		Transform transform, const Rect& rect, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Origin draw_origin = Origin::Center,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawRoundedRect(
		Transform transform, const RoundedRect& rounded_rect, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Origin draw_origin = Origin::Center,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawLine(
		Transform transform, const Line& line, Color color, float line_width = kMinLineWidth,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawLine(
		V2_float start, V2_float end, Color color, float line_width = kMinLineWidth,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawTriangle(
		Transform transform, const Triangle& triangle, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawEllipse(
		Transform transform, const Ellipse& ellipse, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawCircle(
		Transform transform, const Circle& circle, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawCapsule(
		Transform transform, const Capsule& capsule, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawArc(
		Transform transform, const Arc& arc, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawPolygon(
		Transform transform, const Polygon& polygon, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, const std::optional<Camera>& camera = {}
	);

	void DrawPoint(
		V2_float point, Color color, Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		const std::optional<Camera>& camera = {}
	);

private:
	friend class Scene;
	friend class SceneContext;
	friend class DebugContext;

	RenderContext() = delete;
	RenderContext(Scene& scene, Renderer& renderer);
	~RenderContext() noexcept						   = default;
	RenderContext(const RenderContext&)				   = delete;
	RenderContext& operator=(const RenderContext&)	   = delete;
	RenderContext(RenderContext&&) noexcept			   = default;
	RenderContext& operator=(RenderContext&&) noexcept = delete;

	void DrawTexture(
		impl::TextureId texture, V2_int texture_size, impl::ShaderId shader, Transform transform,
		std::optional<V2_float> size, Origin draw_origin, std::optional<Color> tint, Depth depth,
		std::optional<BlendMode> blend_mode,
		const std::optional<std::array<V2_float, 4>>& texture_coordinates,
		const std::optional<Camera>& camera
	);

	template <typename T, typename R>
	static void AddDrawCommand(T& commands, const R& command, float depth) {
		if constexpr (SpecializationOf<R, std::vector>) {
			for (const auto& c : command) {
				commands.emplace_back(c, depth);
			}
		} else {
			commands.emplace_back(command, depth);
		}
	}

	/// @brief If camera is {}, returns draw commands for the primary scene camera. If draw commands
	/// do not exist for the camera, adds them to the vector.
	std::vector<impl::DrawCommand>& GetDrawCommandsForCamera(const std::optional<Camera>& camera);
	std::vector<impl::ManualDrawCommand>& GetDebugCommandsForCamera(
		const std::optional<Camera>& camera
	);

	Scene& scene_;
	Renderer& renderer_;

	std::vector<std::pair<Camera, std::vector<impl::DrawCommand>>> draw_commands_;
	std::vector<std::pair<Camera, std::vector<impl::ManualDrawCommand>>> debug_commands_;

	std::vector<impl::TextureObject> temporary_textures_;
};

} // namespace ptgn