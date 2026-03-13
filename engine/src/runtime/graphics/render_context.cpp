#include "runtime/graphics/render_context.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
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
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "renderer/primitives/viewport.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/font_system.h"
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

static float NormalizeArcLineWidthToThickness(float line_width, float fade, V2_float radii) {
	if (line_width == -1.0f) {
		// Internally line width for a filled SDF is 1.0f.
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width >= kMinLineWidth, "Invalid line width for circle");

		// Internally line width for a completely hollow ellipse is 0.0f.
		line_width = fade + line_width / std::min(radii.x, radii.y);
	}
	return line_width;
}

static float GetAspectRatio(V2_float size) {
	PTGN_ASSERT(size.x > 0.0f);
	return size.y / size.x;
}

static float GetNormalizedRadius(float diameter, float size_x) {
	PTGN_ASSERT(size_x > 0.0f);
	float normalized_radius{ diameter / size_x };
	return std::clamp(normalized_radius, 0.0f, 1.0f);
}

DrawContext::DrawContext(Renderer& renderer) : renderer_{ renderer } {}

std::vector<impl::QuadCommand> DrawContext::GetLineDrawCommands(
	std::span<const V2_float> points, float line_width, Transform transform, Color tint,
	std::optional<BlendMode> blend_mode, bool connect_last_to_first
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Invalid line width for lines");

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

		cmds.emplace_back(line_points, tint, blend_mode);
	}

	return cmds;
}

std::variant<
	std::monostate, impl::QuadCommand, impl::QuadShapeCommand, std::vector<impl::QuadCommand>,
	std::vector<impl::TriangleCommand>>
DrawContext::GetShapeDrawCommand(
	Renderer& renderer, const Shape& shape, Transform transform, Color tint, FillStyle fill_style,
	Origin draw_origin, std::optional<BlendMode> blend_mode
) {
	float line_width{ 0.0f };

	if (std::holds_alternative<impl::Solid>(fill_style.style)) {
		PTGN_ASSERT(
			!std::holds_alternative<Line>(shape),
			"Cannot draw a solid line, use FillStyle::Hollow(line_width)"
		);
		line_width = -1.0f;
	} else {
		if (auto width{ std::get<impl::Hollow>(fill_style.style).line_width }; width >= 1.0f) {
			line_width = width;
		} else {
			return {};
		}
	}

	PTGN_ASSERT(line_width != 0.0f);

	return std::visit(
		[&](const auto& s) -> std::variant<
							   std::monostate, impl::QuadCommand, impl::QuadShapeCommand,
							   std::vector<impl::QuadCommand>, std::vector<impl::TriangleCommand>> {
			using T = std::decay_t<decltype(s)>;

			if constexpr (std::is_same_v<T, Rect>) {
				if (auto size{ s.GetSize(transform) }; !size.BothAboveZero()) {
					return {};
				}

				if (line_width == -1.0f) {
					auto vertices{ s.GetWorldVertices(transform, draw_origin) };
					return impl::QuadCommand{ vertices, tint, blend_mode };
				} else {
					auto vertices{ s.GetWorldVertices(transform, draw_origin) };
					return GetLineDrawCommands(vertices, line_width, {}, tint, blend_mode, true);
				}
			} else if constexpr (std::is_same_v<T, Circle>) {
				Ellipse ellipse{ V2_float{ s.GetRadius() } };
				return GetShapeDrawCommand(
					renderer, ellipse, transform, tint, fill_style, draw_origin, blend_mode
				);
			} else if constexpr (std::is_same_v<T, Line>) {
				auto vertices{ s.GetLocalVertices() };
				return GetLineDrawCommands(
					vertices, line_width, transform, tint, blend_mode, false
				);
			} else if constexpr (std::is_same_v<T, Triangle>) {
				auto triangle{ s.GetWorldVertices(transform) };
				std::array<V2_float, 4> points{ triangle[0], triangle[1], triangle[2],
												triangle[0] };

				if (line_width == -1.0f) {
					return impl::QuadCommand{ points, tint, blend_mode };
				} else {
					auto vertices{ s.GetLocalVertices() };
					return GetLineDrawCommands(
						vertices, line_width, transform, tint, blend_mode, true
					);
				}

			} else if constexpr (std::is_same_v<T, Polygon>) {
				auto vertices{ s.GetLocalVertices() };

				if (vertices.size() < 3) {
					if (vertices.empty()) {
						return {};
					} else if (vertices.size() == 1) {
						return GetShapeDrawCommand(
							renderer, vertices.front(), transform, tint, fill_style, draw_origin,
							blend_mode
						);
					} else if (vertices.size() == 2) {
						return GetShapeDrawCommand(
							renderer, Line{ vertices[0], vertices[1] }, transform, tint, fill_style,
							draw_origin, blend_mode
						);
					}
				}

				if (line_width == -1.0f) {
					auto points{ s.GetWorldVertices(transform) };
					auto triangles{ impl::Triangulate(points) };

					std::vector<impl::TriangleCommand> triangle_commands;

					for (const auto& triangle : triangles) {
						triangle_commands.emplace_back(triangle, tint, blend_mode);
					}

					return triangle_commands;
				} else {
					return GetLineDrawCommands(
						vertices, line_width, transform, tint, blend_mode, true
					);
				}
			} else if constexpr (std::is_same_v<T, V2_float>) {
				Rect rect{ V2_float{ 1.0f } };
				transform.Translate(s);
				auto positions{ rect.GetWorldVertices(transform, Origin::Center) };

				return impl::QuadCommand{ positions, tint, blend_mode };
			} else if constexpr (std::is_same_v<T, Capsule>) {
				auto radius{ s.GetRadius(transform) };

				if (radius <= 0.0f) {
					return {};
				}

				V2_float size;
				auto positions{ s.GetWorldQuadVertices(transform, &size) };

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
				float aspect_ratio{ GetAspectRatio(size) };
				auto thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};

				std::array<float, 4> data{ thickness, fade, normalized_radius, aspect_ratio };

				auto capsule_shader{ renderer.GetShader("capsule") };

				return impl::QuadShapeCommand{ capsule_shader, positions, data, tint, blend_mode };
			} else if constexpr (std::is_same_v<T, Arc>) {
				auto radius{ s.GetRadius(transform) };

				if (radius <= 0.0f) {
					return {};
				}

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};
				float aperture{ s.GetAperture() };
				float direction{ s.clockwise ? 1.0f : -1.0f };
				std::array<float, 4> data{ thickness, fade, aperture, direction };

				transform.Rotate(s.GetStartAngle());

				auto positions{ s.GetWorldQuadVertices(transform) };

				auto arc_shader{ renderer.GetShader("arc") };

				return impl::QuadShapeCommand{ arc_shader, positions, data, tint, blend_mode };
			} else if constexpr (std::is_same_v<T, RoundedRect>) {
				auto size = s.GetSize(transform);

				if (!size.BothAboveZero()) {
					return {};
				}

				float radius = s.GetRadius(transform);

				if (radius <= 0.0f) {
					return GetShapeDrawCommand(
						renderer, Rect{ s.GetSize() }, transform, tint, fill_style, draw_origin,
						blend_mode
					);
				}

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
				float aspect_ratio{ GetAspectRatio(size) };
				auto thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};
				std::array<float, 4> data{ thickness, fade, normalized_radius, aspect_ratio };

				auto positions{ s.GetWorldQuadVertices(transform, draw_origin) };

				auto rounded_rect_shader{ renderer.GetShader("rounded_rect") };

				return impl::QuadShapeCommand{ rounded_rect_shader, positions, data, tint,
											   blend_mode };
			} else if constexpr (std::is_same_v<T, Ellipse>) {
				auto radius = s.GetRadius(transform);

				if (!radius.BothAboveZero()) {
					return {};
				}

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};
				std::array<float, 4> data{ thickness, fade, 0.0f, 0.0f };

				auto positions{ s.GetWorldQuadVertices(transform) };

				auto circle_shader{ renderer.GetShader("circle") };

				return impl::QuadShapeCommand{ circle_shader, positions, data, tint, blend_mode };
			}
		},
		shape
	);
}

void DrawContext::DrawLine(
	impl::ShaderId shader, const std::array<V2_float, 2>& positions, Color tint, float depth
) {
	renderer_.DrawLine(shader, positions, tint, depth);
}

void DrawContext::DrawTriangle(
	impl::ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
) {
	renderer_.DrawTriangle(shader, positions, tint, depth);
}

void DrawContext::DrawQuad(
	impl::ShaderId shader, const std::array<V2_float, 4>& positions,
	const std::array<float, 4>& user_data, Color tint, float depth
) {
	renderer_.DrawQuad(shader, positions, user_data, tint, depth);
}

void DrawContext::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
	Color tint, float depth, const std::array<V2_float, 4>& tex_coords
) {
	renderer_.DrawTexture(shader, texture, positions, tint, depth, tex_coords);
}

void DrawContext::DrawTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
	const std::array<V2_float, 4>& tex_coords
) {
	auto quad_shader{ GetShader("quad") };
	DrawTexture(quad_shader, texture, positions, tint, depth, tex_coords);
}

void DrawContext::DrawQuad(const std::array<V2_float, 4>& positions, Color tint, float depth) {
	auto white_texture{ GetWhiteTexture() };
	auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };
	DrawTexture(white_texture, positions, tint, depth, tex_coords);
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

void DrawContext::SetScissor(const ScissorState& scissor) {
	renderer_.SetScissor(scissor);
}

void DrawContext::SetColorMask(const ColorMaskState& color_mask) {
	renderer_.SetColorMask(color_mask);
}

impl::RenderPass DrawContext::BeginPass(const impl::RenderTargetData& scene_target) {
	return renderer_.BeginPass(scene_target);
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
	DrawTexture(cmd.shader, cmd.texture, cmd.positions, cmd.tint, depth, cmd.tex_coords);
}

void DrawContext::Draw(const impl::QuadCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	DrawQuad(cmd.positions, cmd.color, depth);
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
	DrawQuad(cmd.shader, cmd.positions, cmd.user_data, cmd.color, depth);
}

void DrawContext::Draw(const impl::LineCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	auto color_shader{ GetShader("color") };
	DrawLine(color_shader, cmd.positions, cmd.color, depth);
}

void DrawContext::Draw(const impl::TriangleCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlend(*cmd.blend_mode);
	}
	auto color_shader{ GetShader("color") };
	DrawTriangle(color_shader, cmd.positions, cmd.color, depth);
}

void DrawContext::Draw(const std::vector<impl::TriangleCommand>& cmds, float depth) {
	for (const auto& triangle_cmd : cmds) {
		Draw(triangle_cmd, depth);
	}
}

void DrawContext::Draw(std::monostate, float depth) const { /* No-op */ }

void DrawContext::DrawTexture(
	Texture texture, Transform transform, V2_float size, Origin draw_origin, Color tint,
	float depth, const std::array<V2_float, 4>& tex_coords, std::optional<BlendMode> blend_mode
) {
	if (blend_mode.has_value()) {
		SetBlend(*blend_mode);
	}
	Rect rect{ size };
	auto positions{ rect.GetWorldVertices(transform, draw_origin) };
	DrawTexture(texture, positions, tint, depth, tex_coords);
}

void DrawContext::DrawLines(
	std::span<const V2_float> points, float line_width, Transform transform, Color tint,
	float depth, std::optional<BlendMode> blend_mode, bool connect_last_to_first
) {
	auto line_draw_commands{
		GetLineDrawCommands(points, line_width, transform, tint, blend_mode, connect_last_to_first)
	};

	for (const auto& line : line_draw_commands) {
		Draw(line, depth);
	}
}

void DrawContext::DrawShape(
	const Shape& shape, Transform transform, Color tint, FillStyle fill_style, Origin draw_origin,
	float depth, std::optional<BlendMode> blend_mode
) {
	auto shape_draw_commands{
		GetShapeDrawCommand(renderer_, shape, transform, tint, fill_style, draw_origin, blend_mode)
	};

	std::visit([&](const auto& cmd) { Draw(cmd, depth); }, shape_draw_commands);
}

void RenderContext::Init(Scene& scene, Renderer& renderer) {
	scene_	  = &scene;
	renderer_ = &renderer;
}

std::vector<impl::DrawCommand>& RenderContext::GetDrawCommandsForCamera(std::optional<Camera> camera
) {
	Camera cam{ camera.value_or(scene_->camera) };

	PTGN_ASSERT(cam);

	for (auto& [c, commands] : draw_commands_) {
		if (c == cam) {
			return commands;
		}
	}
	return draw_commands_.emplace_back(cam, std::vector<impl::DrawCommand>{}).second;
}

std::vector<impl::ManualDrawCommand>& RenderContext::GetDebugCommandsForCamera(
	std::optional<Camera> camera
) {
	Camera cam{ camera.value_or(scene_->camera) };

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
	const std::optional<std::array<V2_float, 4>>& texture_coordinates, std::optional<Camera> camera
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	PTGN_ASSERT(renderer_ != nullptr, "Render context must be initialized before use");

	Rect rect{ size.value_or(V2_float{ texture_size }) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	std::array<V2_float, 4> tex_coords;

	if (texture_coordinates.has_value()) {
		tex_coords = *texture_coordinates;
	} else {
		tex_coords = impl::GetDefaultTextureCoordinates<false>();
	}

	impl::TextureCommand texture_command{ shader,	  texture,
										  positions,  tint.value_or(color::White),
										  tex_coords, blend_mode };

	draw_commands.emplace_back(texture_command, depth);
}

void RenderContext::DrawTexture(
	Texture texture, Transform transform, std::optional<V2_float> size, Origin draw_origin,
	std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates, std::optional<Camera> camera
) {
	auto quad_shader{ renderer_->GetShader("quad") };
	auto texture_size{ texture.GetSize() };

	DrawTexture(
		texture, texture_size, quad_shader, transform, size, draw_origin, tint, depth, blend_mode,
		texture_coordinates, camera
	);
}

void RenderContext::DrawTexture(
	Texture texture, Shader shader, Transform transform, std::optional<V2_float> size,
	Origin draw_origin, std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	const std::optional<std::array<V2_float, 4>>& texture_coordinates, std::optional<Camera> camera
) {
	auto texture_size{ texture.GetSize() };

	DrawTexture(
		texture, texture_size, shader, transform, size, draw_origin, tint, depth, blend_mode,
		texture_coordinates, camera
	);
}

void RenderContext::DrawShader(
	Shader shader, Transform transform, std::optional<V2_float> size, Origin draw_origin,
	std::optional<Color> tint, Depth depth, std::optional<BlendMode> blend_mode,
	std::optional<Camera> camera, const std::optional<std::array<float, 4>>& user_data
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	PTGN_ASSERT(renderer_ != nullptr, "Render context must be initialized before use");

	Rect rect{ size.value_or(renderer_->GetGameSize()) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	auto data{ user_data.value_or(std::array<float, 4>{}) };

	impl::QuadShapeCommand quad_shape_command{ shader, positions, data, tint.value_or(color::White),
											   blend_mode };

	draw_commands.emplace_back(quad_shape_command, depth);
}

void RenderContext::DrawLines(
	const std::vector<V2_float>& points, Color color, float line_width, bool connect_last_to_first,
	std::optional<Transform> transform, Depth depth, std::optional<BlendMode> blend_mode,
	std::optional<Camera> camera
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	auto line_draw_commands{ DrawContext::GetLineDrawCommands(
		points, line_width, transform.value_or(Transform{}), color, blend_mode,
		connect_last_to_first
	) };

	for (const auto& line_command : line_draw_commands) {
		draw_commands.emplace_back(line_command, depth);
	}
}

void RenderContext::DrawShape(
	const Shape& shape, Transform transform, Color color, FillStyle fill_style, Origin draw_origin,
	Depth depth, std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	auto& draw_commands{ GetDrawCommandsForCamera(camera) };

	PTGN_ASSERT(renderer_ != nullptr, "Render context must be initialized before use");

	auto shape_draw_commands{ DrawContext::GetShapeDrawCommand(
		*renderer_, shape, transform, color, fill_style, draw_origin, blend_mode
	) };

	std::visit(
		[&](const auto& cmd) { AddDrawCommand(draw_commands, cmd, depth); }, shape_draw_commands
	);
}

void RenderContext::DrawText(
	std::string_view text_content, Transform transform, Color text_color,
	std::optional<float> font_size,
	const std::variant<std::monostate, Font, std::string_view>& font,
	const TextProperties& properties, Origin draw_origin, std::optional<V2_float> text_size,
	bool hd_text, Depth depth, std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	PTGN_ASSERT(
		scene_ != nullptr && renderer_ != nullptr, "Render context must be initialized before use"
	);

	// TODO: Most of this code is duplicated with DebugContext::DrawText and Text::Draw. Consider
	// moving the common parts to a helper function.

	auto resolved_font{ scene_->app().asset.ToFont(font) };

	float hd_scale{ hd_text ? impl::GetTextScale(*scene_, camera) : 1.0f };

	float resolved_font_size{ font_size.value_or(kDefaultFontSize) };

	if (hd_text) {
		resolved_font_size *= hd_scale;

		auto scale{ impl::GetCameraParentRenderTargetScale(*scene_, camera) };

		transform.Scale(transform.GetScale() / scale);
	}

	auto texture_object{ scene_->app().asset.CreateTextTextureObject(
		text_content, text_color, resolved_font_size, resolved_font.value_or(Font{}), properties,
		hd_scale, hd_text
	) };

	if (!texture_object.has_value()) {
		return;
	}

	auto texture_size{ texture_object->GetSize() };

	auto texture_id{ texture_object->operator impl::TextureId() };

	temporary_textures_.emplace_back(std::move(*texture_object));

	auto quad_shader{ renderer_->GetShader("quad") };

	DrawTexture(
		texture_id, texture_size, quad_shader, transform, text_size, draw_origin, color::White,
		depth, blend_mode, {}, camera
	);
}

void RenderContext::DrawRect(
	Transform transform, const Rect& rect, Color color, FillStyle fill_style, Origin draw_origin,
	Depth depth, std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(rect, transform, color, fill_style, draw_origin, depth, blend_mode, camera);
}

void RenderContext::DrawRoundedRect(
	Transform transform, const RoundedRect& rounded_rect, Color color, FillStyle fill_style,
	Origin draw_origin, Depth depth, std::optional<BlendMode> blend_mode,
	std::optional<Camera> camera
) {
	DrawShape(rounded_rect, transform, color, fill_style, draw_origin, depth, blend_mode, camera);
}

void RenderContext::DrawLine(
	Transform transform, const Line& line, Color color, float line_width, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(
		line, transform, color, FillStyle::Hollow(line_width), Origin::Center, depth, blend_mode,
		camera
	);
}

void RenderContext::DrawLine(
	V2_float start, V2_float end, Color color, float line_width, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(
		Line{ start, end }, {}, color, FillStyle::Hollow(line_width), Origin::Center, depth,
		blend_mode, camera
	);
}

void RenderContext::DrawTriangle(
	Transform transform, const Triangle& triangle, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(triangle, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawEllipse(
	Transform transform, const Ellipse& ellipse, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(ellipse, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawCircle(
	Transform transform, const Circle& circle, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(circle, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawCapsule(
	Transform transform, const Capsule& capsule, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(capsule, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawArc(
	Transform transform, const Arc& arc, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(arc, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawPolygon(
	Transform transform, const Polygon& polygon, Color color, FillStyle fill_style, Depth depth,
	std::optional<BlendMode> blend_mode, std::optional<Camera> camera
) {
	DrawShape(polygon, transform, color, fill_style, Origin::Center, depth, blend_mode, camera);
}

void RenderContext::DrawPoint(
	V2_float point, Color color, Depth depth, std::optional<BlendMode> blend_mode,
	std::optional<Camera> camera
) {
	DrawShape(point, {}, color, FillStyle::Solid(), Origin::Center, depth, blend_mode, camera);
}

} // namespace ptgn