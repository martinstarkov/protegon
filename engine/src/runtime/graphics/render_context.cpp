#include "runtime/graphics/render_context.h"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/blend_mode.h"
#include "core/graphics/color.h"
#include "renderer/resources/id.h"
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/vertex/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"

namespace ptgn {

static float GetFade(float diameter_y) {
	PTGN_ASSERT(diameter_y > 0.0f, "Diameter cannot be negative or zero");
	constexpr float fade_scaling_constant{ 0.12f };
	return fade_scaling_constant / diameter_y;
}

static float GetFade(V2_float diameter) {
	return GetFade(diameter.y);
}

static float GetAspectRatio(V2_float size) {
	PTGN_ASSERT(size.x > 0.0f);
	return size.y / size.x;
}

static float GetNormalizedRadius(float diameter, float size_x) {
	PTGN_ASSERT(size_x > 0.0f);
	float normalized_radius{ diameter / size_x };
	return Clamp01(normalized_radius);
}

DrawContext::DrawContext(Renderer& renderer) : renderer_{ renderer } {}

impl::ShaderId DrawContext::GetShaderId(ShaderVariant shader) const {
	return std::visit(
		[&]<typename T>(const T& arg) -> impl::ShaderId {
			if constexpr (std::is_same_v<T, Shader>) {
				return arg;
			} else if constexpr (std::is_same_v<T, impl::ShaderId>) {
				return arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				return renderer_.GetShader(arg);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		shader
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	std::span<const V2_float> points, float line_width, Transform transform, Color tint,
	std::optional<BlendMode> blend_mode, bool connect_last_to_first, bool floor_positions
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);
	;

	std::size_t count{ points.size() };

	PTGN_ASSERT(
		(connect_last_to_first && count >= 3) || (!connect_last_to_first && count >= 2),
		"There must be at least two points to draw a line, and three to connect back to the first "
		"point"
	);

	std::size_t vertex_modulo{ count };

	if (!connect_last_to_first) {
		vertex_modulo -= 1;
	}

	std::vector<impl::QuadCommand> cmds;

	for (std::size_t i = 0; i < count; ++i) {
		Line l{ points[i], points[(i + 1) % vertex_modulo] };
		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };

		cmds.emplace_back(line_points, tint, blend_mode, floor_positions);
	}

	return cmds;
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	const Rect& rect, Transform transform, FillStyle fill_style, Origin draw_origin, Color tint,
	std::optional<BlendMode> blend_mode, bool floor_positions
) {
	if (auto size{ rect.GetSize(transform) }; !size.BothAboveZero()) {
		return std::nullopt;
	}

	auto vertices{ rect.GetWorldVertices(transform, draw_origin) };

	return fill_style.Apply(
		[&]() -> std::optional<impl::DrawCommandType> {
			return impl::QuadCommand{ vertices, tint, blend_mode, floor_positions };
		},
		[&](float line_width) {
			return GetDrawCommand(
				vertices, line_width, Transform{}, tint, blend_mode, true, floor_positions
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	const Triangle& triangle, Transform transform, FillStyle fill_style, Color tint,
	std::optional<BlendMode> blend_mode, bool floor_positions
) {
	return fill_style.Apply(
		[&]() -> std::optional<impl::DrawCommandType> {
			auto vertices{ triangle.GetWorldQuadVertices(transform) };
			return impl::QuadCommand{ vertices, tint, blend_mode, floor_positions };
		},
		[&](float line_width) {
			auto vertices{ triangle.GetLocalVertices() };
			return GetDrawCommand(
				vertices, line_width, transform, tint, blend_mode, true, floor_positions
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	V2_float point, Transform transform, Color tint, std::optional<BlendMode> blend_mode,
	bool floor_positions
) {
	Rect rect{ V2_float{ 1.0f } };
	transform.Translate(point);
	auto vertices{ rect.GetWorldVertices(transform, Origin::Center) };
	return impl::QuadCommand{ vertices, tint, blend_mode, floor_positions };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId capsule_shader, const Capsule& capsule, Transform transform,
	FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
) {
	float radius{ capsule.GetRadius(transform) };

	if (radius <= 0.0f) {
		return std::nullopt;
	}

	V2_float size;
	auto vertices{ capsule.GetWorldQuadVertices(transform, &size) };

	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
	float aspect_ratio{ GetAspectRatio(size) };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	std::array<float, 4> data{ thickness, fade, normalized_radius, aspect_ratio };

	return impl::QuadShapeCommand{
		capsule_shader, vertices, data, tint, blend_mode, floor_positions
	};
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId arc_shader, const Arc& arc, Transform transform, FillStyle fill_style,
	Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
) {
	float radius{ arc.GetRadius(transform) };

	if (radius <= 0.0f) {
		return std::nullopt;
	}

	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	auto aperture{ arc.GetAperture() };
	float direction{ arc.IsClockwise() ? 1.0f : -1.0f };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	std::array<float, 4> data{ thickness, fade, aperture.ToRad().value, direction };

	transform.Rotate(arc.GetStartAngle());

	auto vertices{ arc.GetWorldQuadVertices(transform) };

	return impl::QuadShapeCommand{ arc_shader, vertices, data, tint, blend_mode, floor_positions };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId circle_shader, const Ellipse& ellipse, Transform transform, FillStyle fill_style,
	Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
) {
	V2_float radius{ ellipse.GetRadius(transform) };

	if (!radius.BothAboveZero()) {
		return std::nullopt;
	}

	V2_float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	std::array<float, 4> data{ thickness, fade, 0.0f, 0.0f };

	auto vertices{ ellipse.GetWorldQuadVertices(transform) };

	return impl::QuadShapeCommand{
		circle_shader, vertices, data, tint, blend_mode, floor_positions
	};
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId circle_shader, const Circle& circle, Transform transform, FillStyle fill_style,
	Color tint, std::optional<BlendMode> blend_mode, bool floor_positions
) {
	Ellipse ellipse{ V2_float{ circle.GetRadius() } };
	return GetDrawCommand(
		circle_shader, ellipse, transform, fill_style, tint, blend_mode, floor_positions
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId rounded_rect_shader, const RoundedRect& rounded_rect, Transform transform,
	FillStyle fill_style, Origin draw_origin, Color tint, std::optional<BlendMode> blend_mode,
	bool floor_positions
) {
	V2_float size{ rounded_rect.GetSize(transform) };

	if (!size.BothAboveZero()) {
		return std::nullopt;
	}

	float radius{ rounded_rect.GetRadius(transform) };

	if (radius <= 0.0f) {
		return GetDrawCommand(
			Rect{ rounded_rect.GetSize() }, transform, fill_style, draw_origin, tint, blend_mode,
			floor_positions
		);
	}

	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
	float aspect_ratio{ GetAspectRatio(size) };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	std::array<float, 4> data{ thickness, fade, normalized_radius, aspect_ratio };

	auto vertices{ rounded_rect.GetWorldQuadVertices(transform, draw_origin) };

	return impl::QuadShapeCommand{ rounded_rect_shader, vertices,		data, tint,
								   blend_mode,			floor_positions };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	const Polygon& polygon, Transform transform, FillStyle fill_style, Color tint,
	std::optional<BlendMode> blend_mode, bool floor_positions
) {
	auto vertices{ polygon.GetLocalVertices() };

	if (vertices.empty()) {
		return std::nullopt;
	}

	if (vertices.size() == 1) {
		return GetDrawCommand(vertices.front(), transform, tint, blend_mode, floor_positions);
	}

	if (vertices.size() == 2) {
		return GetDrawCommand(
			Line{ vertices[0], vertices[1] }, transform, fill_style, tint, blend_mode,
			floor_positions
		);
	}

	return fill_style.Apply(
		[&]() -> std::optional<impl::DrawCommandType> {
			auto world_vertices{ polygon.GetWorldVertices(transform) };
			auto triangles{ impl::Triangulate(world_vertices) };

			std::vector<impl::TriangleCommand> triangle_commands;

			for (const auto& triangle : triangles) {
				triangle_commands.emplace_back(triangle, tint, blend_mode, floor_positions);
			}

			return triangle_commands;
		},
		[&](float line_width) {
			return GetDrawCommand(
				vertices, line_width, transform, tint, blend_mode, true, floor_positions
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	const Line& line, Transform transform, FillStyle fill_style, Color tint,
	std::optional<BlendMode> blend_mode, bool floor_positions
) {
	return fill_style.Apply(
		[]() -> std::optional<impl::DrawCommandType> { PTGN_ERROR("Cannot draw solid line"); },
		[&](float line_width) {
			auto vertices{ line.GetLocalVertices() };
			return GetDrawCommand(
				vertices, line_width, transform, tint, blend_mode, false, floor_positions
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	const Renderer& renderer, const Shape& shape, Transform transform, Color tint,
	FillStyle fill_style, Origin draw_origin, std::optional<BlendMode> blend_mode
) {
	constexpr bool floor_positions{ false };

	return shape.Visit([&]<typename T>(const T& s) -> std::optional<impl::DrawCommandType> {
		if constexpr (std::is_same_v<T, Rect>) {
			return GetDrawCommand(
				s, transform, fill_style, draw_origin, tint, blend_mode, floor_positions
			);
		} else if constexpr (IsAnyOf<T, Polygon, Line, Triangle>) {
			return GetDrawCommand(s, transform, fill_style, tint, blend_mode, floor_positions);
		} else if constexpr (std::is_same_v<T, V2_float>) {
			return GetDrawCommand(s, transform, tint, blend_mode, floor_positions);
		} else if constexpr (IsAnyOf<T, Circle, Ellipse>) {
			return GetDrawCommand(
				renderer.GetShader("circle"), s, transform, fill_style, tint, blend_mode,
				floor_positions
			);
		} else if constexpr (std::is_same_v<T, Capsule>) {
			return GetDrawCommand(
				renderer.GetShader("capsule"), s, transform, fill_style, tint, blend_mode,
				floor_positions
			);
		} else if constexpr (std::is_same_v<T, Arc>) {
			return GetDrawCommand(
				renderer.GetShader("arc"), s, transform, fill_style, tint, blend_mode,
				floor_positions
			);
		} else if constexpr (std::is_same_v<T, RoundedRect>) {
			return GetDrawCommand(
				renderer.GetShader("rounded_rect"), s, transform, fill_style, draw_origin, tint,
				blend_mode, floor_positions
			);
		} else {
			static_assert(false, "Incomplete visitor!");
		}
	});
}

void DrawContext::Flush() {
	renderer_.FlushBatch();
}

void DrawContext::DrawTriangle(
	impl::ShaderId shader, std::array<V2_float, 3> positions, Color tint, float depth,
	bool floor_positions
) {
	if (floor_positions) {
		for (auto& pos : positions) {
			pos = FastFloor(pos);
		}
	}
	renderer_.DrawTriangle(shader, positions, tint, depth);
}

void DrawContext::DrawQuad(
	impl::ShaderId shader, std::array<V2_float, 4> positions, const std::array<float, 4>& user_data,
	Color tint, float depth, const std::function<void()>& shader_setup, bool floor_positions
) {
	if (floor_positions) {
		for (auto& pos : positions) {
			pos = FastFloor(pos);
		}
	}
	renderer_.DrawQuad(shader, positions, user_data, tint, depth, shader_setup);
}

void DrawContext::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, std::array<V2_float, 4> positions, Color tint,
	float depth, const std::array<V2_float, 4>& tex_coords,
	const std::function<void()>& shader_setup, bool floor_positions
) {
	if (floor_positions) {
		for (auto& pos : positions) {
			pos = FastFloor(pos);
		}
	}
	renderer_.DrawTexture(shader, texture, positions, tint, depth, tex_coords, shader_setup);
}

void DrawContext::DrawTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
	const std::array<V2_float, 4>& tex_coords, bool floor_positions
) {
	auto quad_shader{ GetShader("quad") };
	DrawTexture(quad_shader, texture, positions, tint, depth, tex_coords, {}, floor_positions);
}

void DrawContext::DrawQuad(
	const std::array<V2_float, 4>& positions, Color tint, float depth, bool floor_positions
) {
	auto white_texture{ GetWhiteTexture() };
	auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };
	DrawTexture(white_texture, positions, tint, depth, tex_coords, floor_positions);
}

void DrawContext::DrawTexture(
	Texture texture, Transform transform, V2_float size, Origin draw_origin, Color tint,
	float depth, const std::array<V2_float, 4>& tex_coords, std::optional<BlendMode> blend_mode
) {
	if (blend_mode.has_value()) {
		SetBlend(*blend_mode);
	}
	Rect rect{ size };
	auto positions{ rect.GetWorldVertices(transform, draw_origin) };
	constexpr bool floor_positions{ true };
	DrawTexture(texture, positions, tint, depth, tex_coords, floor_positions);
}

void DrawContext::BindScreenTarget() {
	return renderer_.BindScreenTarget();
}

void DrawContext::SetViewport(Viewport viewport) {
	renderer_.SetViewport(viewport);
}

void DrawContext::SetViewProjection(const Matrix4& view_projection) {
	renderer_.SetViewProjection(view_projection);
}

void DrawContext::SetBlend(BlendMode mode, bool enabled) {
	renderer_.SetBlend(mode, enabled);
}

void DrawContext::SetDepth(const DepthState& depth) {
	renderer_.SetDepth(depth);
}

void DrawContext::SetStencil(const StencilState& stencil) {
	renderer_.SetStencil(stencil);
}

void DrawContext::SetRaster(const RasterState& raster) {
	renderer_.SetRaster(raster);
}

void DrawContext::SetShader(const Shader& shader) {
	renderer_.SetShader(shader);
}

void DrawContext::SetShader(impl::ShaderId shader) {
	renderer_.SetShader(shader);
}

void DrawContext::SetScissor(const ScissorState& scissor) {
	renderer_.SetScissor(scissor);
}

void DrawContext::SetColorMask(const ColorMaskState& color_mask) {
	renderer_.SetColorMask(color_mask);
}

impl::RenderPass DrawContext::BeginPass(impl::RenderTargetId scene_render_target) {
	return renderer_.BeginPass(scene_render_target);
}

impl::TextureId DrawContext::GetWhiteTexture() const {
	return renderer_.GetWhiteTexture();
}

impl::ShaderId DrawContext::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

void DrawContext::Draw(const impl::ManualCommand& command, float depth) {
	std::visit([&](auto& cmd) { Draw(cmd, depth); }, command);
}

void DrawContext::Draw(const impl::TextureCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	DrawTexture(
		cmd.shader, cmd.texture, cmd.positions, cmd.tint, depth, cmd.tex_coords, {},
		cmd.floor_positions
	);
}

void DrawContext::Draw(const impl::QuadCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	DrawQuad(cmd.positions, cmd.color, depth, cmd.floor_positions);
}

void DrawContext::Draw(const std::vector<impl::QuadCommand>& cmds, float depth) {
	for (const auto& quad_cmd : cmds) {
		Draw(quad_cmd, depth);
	}
}

void DrawContext::Draw(const impl::QuadShapeCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	DrawQuad(cmd.shader, cmd.positions, cmd.user_data, cmd.color, depth, {}, cmd.floor_positions);
}

void DrawContext::Draw(const impl::TriangleCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	auto color_shader{ GetShader("color") };
	DrawTriangle(color_shader, cmd.positions, cmd.color, depth, cmd.floor_positions);
}

void DrawContext::Draw(const std::vector<impl::TriangleCommand>& cmds, float depth) {
	for (const auto& triangle_cmd : cmds) {
		Draw(triangle_cmd, depth);
	}
}

void DrawContext::DrawLines(
	std::span<const V2_float> points, float line_width, Transform transform, Color tint,
	float depth, std::optional<BlendMode> blend_mode, bool connect_last_to_first
) {
	constexpr bool floor_positions{ false };

	auto draw_commands{ GetDrawCommand(
		points, line_width, transform, tint, blend_mode, connect_last_to_first, floor_positions
	) };

	if (!draw_commands.has_value()) {
		return;
	}

	PTGN_ASSERT(std::holds_alternative<std::vector<impl::QuadCommand>>(*draw_commands));

	const auto& line_draw_commands{ std::get<std::vector<impl::QuadCommand>>(*draw_commands) };

	for (const auto& line : line_draw_commands) {
		Draw(line, depth);
	}
}

void DrawContext::DrawShape(
	const Shape& shape, Transform transform, Color tint, FillStyle fill_style, Origin draw_origin,
	float depth, std::optional<BlendMode> blend_mode
) {
	auto shape_draw_commands{
		GetDrawCommand(renderer_, shape, transform, tint, fill_style, draw_origin, blend_mode)
	};

	if (!shape_draw_commands.has_value()) {
		return;
	}

	std::visit([&](const auto& cmd) { Draw(cmd, depth); }, *shape_draw_commands);
}

RenderContext::RenderContext(Scene& scene, Renderer& renderer) :
	scene_{ scene }, renderer_{ renderer } {}

std::vector<impl::DrawCommand>& RenderContext::GetDrawCommandsForCamera(
	const std::optional<Camera>& camera
) {
	Camera cam{ camera.value_or(scene_.ctx().camera) };

	PTGN_ASSERT(cam);

	for (auto& [c, commands] : draw_commands_) {
		if (c == cam) {
			return commands;
		}
	}
	return draw_commands_.emplace_back(cam, std::vector<impl::DrawCommand>{}).second;
}

std::vector<impl::ManualDrawCommand>& RenderContext::GetDebugCommandsForCamera(
	const std::optional<Camera>& camera
) {
	Camera cam{ camera.value_or(scene_.ctx().camera) };

	PTGN_ASSERT(cam);

	for (auto& [c, commands] : debug_commands_) {
		if (c == cam) {
			return commands;
		}
	}
	return debug_commands_.emplace_back(cam, std::vector<impl::ManualDrawCommand>{}).second;
}

void RenderContext::DrawTexture(
	impl::TextureId texture, V2_int texture_size, impl::ShaderId shader, Transform transform,
	std::optional<V2_float> size, Origin draw_origin, std::optional<Color> tint, Depth depth,
	std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<Camera>& camera
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	Rect rect{ size.value_or(V2_float{ texture_size }) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	std::array<V2_float, 4> tex_coords{
		texture_coordinates.value_or(impl::GetDefaultTextureCoordinates<false>())
	};

	constexpr bool floor_positions{ true };

	impl::TextureCommand texture_command{ shader,		  texture,
										  positions,	  tint.value_or(color::White),
										  tex_coords,	  blend_mode,
										  floor_positions };

	draw_commands.emplace_back(texture_command, depth);
}

void RenderContext::DrawTexture(
	TextureOrKey texture, Transform transform, std::optional<V2_float> size, Origin draw_origin,
	std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<Camera>& camera
) {
	auto quad_shader{ renderer_.GetShader("quad") };

	const auto& assets{ scene_.ctx().asset };
	auto resolved_texture{ texture.Get(assets) };
	auto texture_size{ resolved_texture.GetSize() };

	DrawTexture(
		resolved_texture, texture_size, quad_shader, transform, size, draw_origin, tint, depth,
		blend_mode, texture_coordinates, camera
	);
}

void RenderContext::DrawTexture(
	TextureOrKey texture, Shader shader, Transform transform, std::optional<V2_float> size,
	Origin draw_origin, std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates,
	const std::optional<Camera>& camera
) {
	const auto& assets{ scene_.ctx().asset };
	auto resolved_texture{ texture.Get(assets) };
	auto texture_size{ resolved_texture.GetSize() };

	DrawTexture(
		resolved_texture, texture_size, shader, transform, size, draw_origin, tint, depth,
		blend_mode, texture_coordinates, camera
	);
}

void RenderContext::DrawShader(
	Shader shader, Transform transform, std::optional<V2_float> size, Origin draw_origin,
	std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<Camera>& camera, const std::optional<std::array<float, 4>>& user_data
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	Rect rect{ size.value_or(renderer_.GetGameSize()) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	auto data{ user_data.value_or(std::array<float, 4>{}) };

	constexpr bool floor_positions{ true };

	impl::QuadShapeCommand quad_shape_command{ shader,	   positions,
											   data,	   tint.value_or(color::White),
											   blend_mode, floor_positions };

	draw_commands.emplace_back(quad_shape_command, depth);
}

void RenderContext::DrawLines(
	const std::vector<V2_float>& points, Color color, float line_width, bool connect_last_to_first,
	std::optional<Transform> transform, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<Camera>& camera
) {
	auto& camera_commands{ GetDrawCommandsForCamera(camera) };

	constexpr bool floor_positions{ false };

	auto draw_commands{ DrawContext::GetDrawCommand(
		points, line_width, transform.value_or(Transform{}), color, blend_mode,
		connect_last_to_first, floor_positions
	) };

	PTGN_ASSERT(std::holds_alternative<std::vector<impl::QuadCommand>>(*draw_commands));

	const auto& line_draw_commands{ std::get<std::vector<impl::QuadCommand>>(*draw_commands) };

	for (const auto& line_command : line_draw_commands) {
		camera_commands.emplace_back(line_command, depth);
	}
}

void RenderContext::DrawShape(
	const Shape& shape, Transform transform, Color color, FillStyle fill_style, Origin draw_origin,
	Depth depth, std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	auto shape_draw_commands{ DrawContext::GetDrawCommand(
		renderer_, shape, transform, color, fill_style, draw_origin, blend_mode
	) };

	if (!shape_draw_commands.has_value()) {
		return;
	}

	std::visit(
		[&](const auto& cmd) { AddDrawCommand(draw_commands, cmd, depth); }, *shape_draw_commands
	);
}

void RenderContext::DrawText(
	std::string_view text_content, Transform transform, Color text_color, FontSize font_size,
	FontOrKey font, const TextProperties& properties, Origin draw_origin,
	std::optional<V2_float> text_size, bool hd_text, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	auto hd_scale{ impl::ApplyHDTextScaling(hd_text, transform, scene_, camera) };

	auto texture_object{ scene_.ctx().asset.CreateTextTextureObject(
		text_content, text_color, font_size, font, properties, hd_scale
	) };

	if (!texture_object.has_value()) {
		return;
	}

	auto texture_size{ texture_object->GetSize() };

	auto texture_id{ texture_object->operator impl::TextureId() };

	temporary_textures_.emplace_back(std::move(*texture_object));

	auto quad_shader{ renderer_.GetShader("quad") };

	DrawTexture(
		texture_id, texture_size, quad_shader, transform, text_size, draw_origin, color::White,
		depth, blend_mode, {}, camera
	);
}

void RenderContext::DrawRect(
	Transform transform, const Rect& rect, Color color, FillStyle fill_style, Origin draw_origin,
	Depth depth, std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(rect, transform, color, fill_style, draw_origin, depth, blend_mode, camera);
}

void RenderContext::DrawRoundedRect(
	Transform transform, const RoundedRect& rounded_rect, Color color, FillStyle fill_style,
	Origin draw_origin, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<Camera>& camera
) {
	DrawShape(rounded_rect, transform, color, fill_style, draw_origin, depth, blend_mode, camera);
}

void RenderContext::DrawLine(
	Transform transform, const Line& line, Color color, float line_width, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);
	DrawShape(line, transform, color, line_width, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawLine(
	V2_float start, V2_float end, Color color, float line_width, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);
	DrawShape(Line{ start, end }, {}, color, line_width, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawTriangle(
	Transform transform, const Triangle& triangle, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(triangle, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawEllipse(
	Transform transform, const Ellipse& ellipse, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(ellipse, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawCircle(
	Transform transform, const Circle& circle, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(circle, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawCapsule(
	Transform transform, const Capsule& capsule, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(capsule, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawArc(
	Transform transform, const Arc& arc, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(arc, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawPolygon(
	Transform transform, const Polygon& polygon, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, const std::optional<Camera>& camera
) {
	DrawShape(polygon, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawPoint(
	V2_float point, Color color, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<Camera>& camera
) {
	DrawShape(point, {}, color, Solid{}, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::SetGameSize(std::optional<V2_int> game_size, ScalingMode scaling_mode) {
	renderer_.SetGameSize(game_size, scaling_mode);
}

void RenderContext::SetScalingMode(ScalingMode scaling_mode) {
	renderer_.SetScalingMode(scaling_mode);
}

V2_int RenderContext::GetDisplaySize() const {
	return renderer_.GetDisplaySize();
}

Viewport RenderContext::GetDisplayViewport() const {
	return renderer_.GetDisplayViewport();
}

V2_float RenderContext::GetScale() const {
	return renderer_.GetScale();
}

V2_int RenderContext::GetGameSize() const {
	return renderer_.GetGameSize();
}

ScalingMode RenderContext::GetScalingMode() const {
	return renderer_.GetScalingMode();
}

void RenderContext::SetBackgroundColor(Color background_color) {
	renderer_.SetBackgroundColor(background_color);
}

Color RenderContext::GetBackgroundColor() const {
	return renderer_.GetBackgroundColor();
}

} // namespace ptgn