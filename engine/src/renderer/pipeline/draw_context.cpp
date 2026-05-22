#include "renderer/pipeline/draw_context.h"

#include <array>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/vertex/vertex.h"

namespace ptgn {

namespace {

float GetFade(float diameter_y) {
	PTGN_ASSERT(diameter_y > 0.0f, "Diameter cannot be negative or zero");
	constexpr float fade_scaling_constant{ 0.12f };
	return fade_scaling_constant / diameter_y;
}

float GetFade(V2_float diameter) {
	return GetFade(diameter.y);
}

float GetAspectRatio(V2_float size) {
	PTGN_ASSERT(size.x > 0.0f);
	return size.y / size.x;
}

float GetNormalizedRadius(float diameter, float size_x) {
	PTGN_ASSERT(size_x > 0.0f);
	float normalized_radius{ diameter / size_x };
	return Clamp01(normalized_radius);
}

} // namespace

DrawContext::DrawContext(impl::Renderer& renderer) : renderer_{ renderer } {}

// V2_int DrawContext::BoundTargetSize() const {
//	return renderer_.GetRenderTargetSize(renderer_.GetCurrentTarget());
// }

// TextureSource DrawContext::BoundTarget() const {
//	return impl::BoundTarget{};
// }

// RenderPassBuilder DrawContext::Pass() {
//	return renderer_.Pass();
// }

void DrawContext::SetBlendMode(BlendMode mode) {
	renderer_.SetBlendMode(mode);
}

void DrawContext::SetShader(std::string_view shader) {
	SetShader(renderer_.GetShader(shader));
}

void DrawContext::SetShader(impl::ShaderId shader) {
	renderer_.SetShader(shader);
}

void DrawContext::DrawTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, float depth, Color tint,
	const std::array<V2_float, 4>& tex_coords, const impl::EffectParams& effects,
	std::span<const impl::TextureBinding> extra_textures, int entity_id
) {
	auto quad{
		impl::CreateRenderQuad(positions, depth, tint.Normalized(), tex_coords, 0.0f, entity_id)
	};

	// TODO: Fix.
	// renderer_.DrawTextures({ &quad, 1 }, { &texture, 1 }, effects, extra_textures);
}

void DrawContext::Draw(const impl::ManualCommand& cmd) {
	std::visit(
		[&]<typename T>(const T& arg) {
			if constexpr (std::is_same_v<T, impl::TriangleCommand>) {
				renderer_.SetCurrentPipeline("color");
				renderer_.SetMaterial({ .shader{ GetShader("color") } });
				// TODO: Fix.
				// renderer_.DrawTriangles<impl::ColorVertex>(
				//	arg.triangles, {}, impl::NoTextureIndexAccessor{}
				//);
			} else if constexpr (std::is_same_v<T, impl::QuadCommand>) {
				renderer_.SetCurrentPipeline("color");
				renderer_.SetMaterial({ .shader{ GetShader("color") } });
				// TODO: Fix.
				// renderer_.DrawQuads<impl::ColorVertex>(
				//	arg.quads, {}, impl::NoTextureIndexAccessor{}
				//);
			} else if constexpr (std::is_same_v<T, impl::ShapeCommand>) {
				renderer_.SetCurrentPipeline("shape");
				renderer_.SetMaterial({ .shader{ arg.shader } });
				// TODO: Fix.
				// renderer_.DrawQuads<impl::ShapeVertex>(
				//	arg.shapes, {}, impl::NoTextureIndexAccessor{}
				//);
			} else if constexpr (std::is_same_v<T, impl::TextureCommand>) {
				renderer_.SetCurrentPipeline("texture");
				renderer_.SetMaterial(arg.material);
				// TODO: Fix.
				// renderer_.DrawQuads<impl::TextureVertex>(
				//	arg.quads, arg.textures, impl::NoTextureIndexAccessor{}
				//);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		cmd
	);
}

impl::ShaderId DrawContext::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

/*

DrawContext::DrawContext(impl::Renderer& renderer) : renderer_{ renderer } {}

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
	impl::ShaderId quad_shader, std::span<const V2_float> points, float line_width,
	Transform transform, Color tint, std::optional<BlendMode> blend_mode,
	bool connect_last_to_first, int entity_id
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);

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

	for (auto i{ 0uz }; i < count; ++i) {
		Line l{ points[i], points[(i + 1) % vertex_modulo] };
		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };

		cmds.emplace_back(quad_shader, line_points, tint, blend_mode, entity_id);
	}

	return cmds;
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId quad_shader, const Rect& rect, Transform transform, FillStyle fill_style,
	Origin draw_origin, Color tint, std::optional<BlendMode> blend_mode, int entity_id
) {
	if (auto size{ rect.GetSize(transform) }; !size.BothAboveZero()) {
		return std::nullopt;
	}

	auto vertices{ rect.GetWorldVertices(transform, draw_origin) };

	return fill_style.Apply(
		[&]() -> std::optional<impl::DrawCommandType> {
			return impl::QuadCommand{ quad_shader, vertices, tint, blend_mode, entity_id };
		},
		[&](float line_width) {
			return GetDrawCommand(
				quad_shader, vertices, line_width, Transform{}, tint, blend_mode, true, entity_id
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId triangle_shader, const Triangle& triangle, Transform transform,
	FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
) {
	return fill_style.Apply(
		[&]() -> std::optional<impl::DrawCommandType> {
			auto vertices{ triangle.GetWorldQuadVertices(transform) };
			return impl::QuadCommand{ triangle_shader, vertices, tint, blend_mode, entity_id };
		},
		[&](float line_width) {
			auto vertices{ triangle.GetLocalVertices() };
			return GetDrawCommand(
				triangle_shader, vertices, line_width, transform, tint, blend_mode, true, entity_id
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId point_shader, V2_float point, Transform transform, Color tint,
	std::optional<BlendMode> blend_mode, int entity_id
) {
	Rect rect{ V2_float{ 1.0f } };
	transform.Translate(point);
	auto vertices{ rect.GetWorldVertices(transform, Origin::Center) };
	return impl::QuadCommand{ point_shader, vertices, tint, blend_mode, entity_id };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId capsule_shader, const Capsule& capsule, Transform transform,
	FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
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

	auto tex_coords{ impl::GetNDCTextureCoordinates() };

	for (auto& coord : tex_coords) {
		coord.y *= aspect_ratio;
	}

	return impl::ShapeCommand{ capsule_shader, vertices,   tint,	 tex_coords,
							   data,		   blend_mode, entity_id };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId arc_shader, const Arc& arc, Transform transform, FillStyle fill_style,
	Color tint, std::optional<BlendMode> blend_mode, int entity_id
) {
	float radius{ arc.GetRadius(transform) };

	if (radius <= 0.0f) {
		return std::nullopt;
	}

	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	float start_angle	  = arc.GetStartAngle().ToRad().value;
	float signed_aperture = arc.GetAperture().ToRad().value * (arc.IsClockwise() ? -1.0f : 1.0f);

	std::array<float, 4> data{ thickness, fade, start_angle, signed_aperture };

	auto vertices{ arc.GetWorldQuadVertices(transform) };

	constexpr auto tex_coords{ impl::GetNDCTextureCoordinates() };

	return impl::ShapeCommand{
		arc_shader, vertices, tint, tex_coords, data, blend_mode, entity_id
	};
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId ellipse_shader, const Ellipse& ellipse, Transform transform,
	FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
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

	constexpr auto tex_coords{ impl::GetNDCTextureCoordinates() };

	return impl::ShapeCommand{ ellipse_shader, vertices,   tint,	 tex_coords,
							   data,		   blend_mode, entity_id };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId circle_shader, const Circle& circle, Transform transform, FillStyle fill_style,
	Color tint, std::optional<BlendMode> blend_mode, int entity_id
) {
	Ellipse ellipse{ V2_float{ circle.GetRadius() } };
	return GetDrawCommand(
		circle_shader, ellipse, transform, fill_style, tint, blend_mode, entity_id
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId rect_shader, impl::ShaderId rounded_rect_shader, const RoundedRect& rounded_rect,
	Transform transform, FillStyle fill_style, Origin draw_origin, Color tint,
	std::optional<BlendMode> blend_mode, int entity_id
) {
	V2_float size{ rounded_rect.GetSize(transform) };

	if (!size.BothAboveZero()) {
		return std::nullopt;
	}

	float radius{ rounded_rect.GetRadius(transform) };

	if (radius <= 0.0f) {
		return GetDrawCommand(
			rect_shader, Rect{ rounded_rect.GetSize() }, transform, fill_style, draw_origin, tint,
			blend_mode, entity_id
		);
	}

	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
	float aspect_ratio{ GetAspectRatio(size) };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	std::array<float, 4> data{ thickness * aspect_ratio, fade, normalized_radius * aspect_ratio,
							   aspect_ratio };

	auto vertices{ rounded_rect.GetWorldQuadVertices(transform, draw_origin) };

	auto tex_coords{ impl::GetNDCTextureCoordinates() };

	for (auto& coord : tex_coords) {
		coord.y *= aspect_ratio;
	}

	return impl::ShapeCommand{ rounded_rect_shader, vertices, tint, tex_coords, data,
							   blend_mode,			entity_id };
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId polygon_shader, const Polygon& polygon, Transform transform,
	FillStyle fill_style, Color tint, std::optional<BlendMode> blend_mode, int entity_id
) {
	auto vertices{ polygon.GetLocalVertices() };

	if (vertices.empty()) {
		return std::nullopt;
	}

	if (vertices.size() == 1) {
		return GetDrawCommand(
			polygon_shader, vertices.front(), transform, tint, blend_mode, entity_id
		);
	}

	if (vertices.size() == 2) {
		return GetDrawCommand(
			polygon_shader, Line{ vertices[0], vertices[1] }, transform, fill_style, tint,
			blend_mode, entity_id
		);
	}

	return fill_style.Apply(
		[&]() -> std::optional<impl::DrawCommandType> {
			auto world_vertices{ polygon.GetWorldVertices(transform) };
			auto triangles{ impl::Triangulate(world_vertices) };

			std::vector<impl::TriangleCommand> triangle_commands;

			for (const auto& triangle : triangles) {
				triangle_commands.emplace_back(triangle, tint, blend_mode, entity_id);
			}

			return triangle_commands;
		},
		[&](float line_width) {
			return GetDrawCommand(
				polygon_shader, vertices, line_width, transform, tint, blend_mode, true, entity_id
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	impl::ShaderId quad_shader, const Line& line, Transform transform, FillStyle fill_style,
	Color tint, std::optional<BlendMode> blend_mode, int entity_id
) {
	return fill_style.Apply(
		[]() -> std::optional<impl::DrawCommandType> { PTGN_ERROR("Cannot draw solid line"); },
		[&](float line_width) {
			auto vertices{ line.GetLocalVertices() };
			return GetDrawCommand(
				quad_shader, vertices, line_width, transform, tint, blend_mode, false, entity_id
			);
		}
	);
}

std::optional<impl::DrawCommandType> DrawContext::GetDrawCommand(
	const impl::Renderer& renderer, const Shape& shape, Transform transform, Color tint,
	FillStyle fill_style, Origin draw_origin, std::optional<BlendMode> blend_mode, int entity_id
) {
	return shape.Visit([&]<typename T>(const T& s) -> std::optional<impl::DrawCommandType> {
		if constexpr (std::is_same_v<T, Rect>) {
			return GetDrawCommand(
				renderer.GetShader("color"), s, transform, fill_style, draw_origin, tint,
				blend_mode, entity_id
			);
		} else if constexpr (IsAnyOf<T, Polygon, Line, Triangle>) {
			return GetDrawCommand(
				renderer.GetShader("color"), s, transform, fill_style, tint, blend_mode, entity_id
			);
		} else if constexpr (std::is_same_v<T, V2_float>) {
			return GetDrawCommand(
				renderer.GetShader("color"), s, transform, tint, blend_mode, entity_id
			);
		} else if constexpr (IsAnyOf<T, Circle, Ellipse>) {
			return GetDrawCommand(
				renderer.GetShader("ellipse"), s, transform, fill_style, tint, blend_mode, entity_id
			);
		} else if constexpr (std::is_same_v<T, Capsule>) {
			return GetDrawCommand(
				renderer.GetShader("capsule"), s, transform, fill_style, tint, blend_mode, entity_id
			);
		} else if constexpr (std::is_same_v<T, Arc>) {
			return GetDrawCommand(
				renderer.GetShader("arc"), s, transform, fill_style, tint, blend_mode, entity_id
			);
		} else if constexpr (std::is_same_v<T, RoundedRect>) {
			return GetDrawCommand(
				renderer.GetShader("color"), renderer.GetShader("rounded_rect"), s, transform,
				fill_style, draw_origin, tint, blend_mode, entity_id
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
	impl::ShaderId shader, const std::array<V2_float, 3>& positions, float depth, Color tint,
	int entity_id
) {
	renderer_.DrawTriangle(shader, positions, depth, tint, entity_id);
}

void DrawContext::DrawQuad(
	impl::ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
	int entity_id
) {
	renderer_.DrawQuad(shader, positions, depth, tint, entity_id);
}

void DrawContext::DrawShape(
	impl::ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
	const std::array<V2_float, 4>& tex_coords, const std::array<float, 4>& shape_data, int entity_id
) {
	renderer_.DrawShape(shader, positions, depth, tint, tex_coords, shape_data, entity_id);
}

void DrawContext::DrawShader(
	impl::ShaderId shader, std::array<V2_float, 4> positions, float depth, Color tint,
	const std::array<V2_float, 4>& tex_coords, const std::function<void()>& shader_setup,
	int entity_id
) {
	for (auto& pos : positions) {
		pos = FastFloor(pos);
	}
	renderer_.DrawShader(shader, positions, depth, tint, tex_coords, shader_setup, entity_id);
}

void DrawContext::DrawTexture(
	impl::TextureId texture, std::array<V2_float, 4> positions, float depth, Color tint,
	const std::array<V2_float, 4>& tex_coords, int entity_id
) {
	DrawTexture(GetShader("texture"), texture, positions, depth, tint, tex_coords, {}, entity_id);
}

void DrawContext::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, std::array<V2_float, 4> positions, float depth,
	Color tint, const std::array<V2_float, 4>& tex_coords,
	const std::function<void()>& shader_setup, int entity_id
) {
	for (auto& pos : positions) {
		pos = FastFloor(pos);
	}
	renderer_.DrawTexture(
		shader, texture, positions, depth, tint, tex_coords, shader_setup, entity_id
	);
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

void DrawContext::SetBlend(bool enabled) {
	renderer_.SetBlend(enabled);
}

void DrawContext::SetBlendMode(BlendMode mode) {
	renderer_.SetBlendMode(mode);
}

void DrawContext::SetDepthTesting(bool enabled) {
	renderer_.SetDepthTesting(enabled);
}

void DrawContext::SetDepthMask(const DepthMaskState& mask) {
	renderer_.SetDepthMask(mask);
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

void DrawContext::Draw(const impl::ManualCommand& command, float depth) {
	std::visit([&](auto& cmd) { Draw(cmd, depth); }, command);
}

void DrawContext::Draw(const impl::TextureCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlendMode(*cmd.blend_mode);
	}
	DrawTexture(
		cmd.shader, cmd.texture, cmd.positions, depth, cmd.color, cmd.tex_coords, {}, cmd.entity_id
	);
}

void DrawContext::Draw(const impl::QuadCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlendMode(*cmd.blend_mode);
	}
	DrawQuad(cmd.shader, cmd.positions, depth, cmd.color, cmd.entity_id);
}

void DrawContext::Draw(const std::vector<impl::QuadCommand>& cmds, float depth) {
	for (const auto& quad_cmd : cmds) {
		Draw(quad_cmd, depth);
	}
}

void DrawContext::Draw(const impl::ShapeCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlendMode(*cmd.blend_mode);
	}
	DrawShape(
		cmd.shader, cmd.positions, depth, cmd.color, cmd.tex_coords, cmd.shape_data, cmd.entity_id
	);
}

void DrawContext::Draw(const impl::TriangleCommand& cmd, float depth) {
	if (cmd.blend_mode.has_value()) {
		SetBlendMode(*cmd.blend_mode);
	}
	auto color_shader{ GetShader("color") };
	DrawTriangle(color_shader, cmd.positions, depth, cmd.color, cmd.entity_id);
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
		GetShader("color"), points, line_width, transform, tint, blend_mode, connect_last_to_first,
		floor_positions
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

void DrawContext::DrawTexture(
	impl::TextureId texture, Transform transform, float depth, V2_float size, Origin draw_origin,
	Color tint, const std::array<V2_float, 4>& tex_coords, std::optional<BlendMode> blend_mode,
	int entity_id
) {
	// TODO: Make this an assert once text is fixed.
	if (!size.BothAboveZero()) {
		return;
	}
	if (blend_mode.has_value()) {
		SetBlendMode(*blend_mode);
	}
	Rect rect{ size };
	auto positions{ rect.GetWorldVertices(transform, draw_origin) };
	DrawTexture(texture, positions, depth, tint, tex_coords, entity_id);
}

void DrawContext::DrawShape(
	const Shape& shape, Transform transform, float depth, Color tint, FillStyle fill_style,
	Origin draw_origin, std::optional<BlendMode> blend_mode, int entity_id
) {
	auto shape_draw_commands{ GetDrawCommand(
		renderer_, shape, transform, tint, fill_style, draw_origin, blend_mode, entity_id
	) };

	if (!shape_draw_commands.has_value()) {
		return;
	}

	std::visit([&](const auto& cmd) { Draw(cmd, depth); }, *shape_draw_commands);
}
*/

} // namespace ptgn