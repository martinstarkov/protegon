
#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/camera/scaling_mode.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/event/event_handler.h"

namespace ptgn {

Renderer::Renderer(Window& window, EventHandler& events) :
	window_{ window },
	events_{ events },
	gl_renderer_{ std::make_shared<impl::gl::GLRenderer>(window) } {}

Renderer::~Renderer() noexcept {
	// Destructor access to impl::gl::GLRenderer is needed.
}

void Renderer::OnEvent(EventDispatcher d) {
	d.Dispatch<WindowResized>([this](auto& e) {
		if (!game_size_) {
			GameResized game_resized;
			game_resized.size = e.size;
			// PTGN_LOG("Emitting game resized: ", e.size);
			events_.Emit(game_resized);
		}

		UpdateDisplayViewport(e.size);
	});
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	if (scaling_mode_ == scaling_mode) {
		return;
	}

	scaling_mode_ = scaling_mode;

	UpdateDisplayViewport(window_.GetSize());
}

void Renderer::SetGameSize(std::optional<V2_int> game_size, ScalingMode scaling_mode) {
	if (game_size_ == game_size && scaling_mode_ == scaling_mode) {
		return;
	}

	game_size_	  = game_size;
	scaling_mode_ = scaling_mode;

	GameResized game_resized;
	game_resized.size = GetGameSize();
	// PTGN_LOG("Emitting game resized: ", game_resized.size);
	events_.Emit(game_resized);
	UpdateDisplayViewport(window_.GetSize());
}

V2_int Renderer::GetDisplaySize() const {
	return display_viewport_.size;
}

V2_float Renderer::GetScale() const {
	auto display_size{ GetDisplaySize() };
	auto game_size{ GetGameSize() };

	PTGN_ASSERT(display_size.BothAboveZero());
	PTGN_ASSERT(game_size.BothAboveZero());

	return V2_float{ display_size } / game_size;
}

V2_int Renderer::GetGameSize() const {
	if (game_size_) {
		return *game_size_;
	}
	return window_.GetSize();
}

ScalingMode Renderer::GetScalingMode() const {
	return scaling_mode_;
}

void Renderer::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
	Color tint, float depth, bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	gl_renderer_->DrawTexture(shader, texture, positions, tint, depth, flip_y, tex_coords);
}

void Renderer::DrawQuadTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
	bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	DrawTexture(GetShader("quad"), texture, positions, tint, depth, flip_y, tex_coords);
}

void Renderer::DrawQuad(
	const std::array<V2_float, 4>& positions, Color tint, float depth,
	const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	DrawQuadTexture(GetWhiteTexture(), positions, tint, depth, false, tex_coords);
}

impl::TextureId Renderer::GetWhiteTexture() const {
	return gl_renderer_->GetWhiteTexture();
}

impl::ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_renderer_->GetShader(name);
}

impl::RenderTargetObject Renderer::CreateRenderTarget(V2_int size, TextureFormat format) {
	return gl_renderer_->CreateRenderTarget(size, format);
}

void Renderer::BindScreenTarget() {
	return gl_renderer_->BindScreenTarget();
}

void Renderer::SetViewport(Viewport viewport) {
	gl_renderer_->SetViewport(viewport);
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	gl_renderer_->SetViewProjection(view_projection);
}

void Renderer::SetBlend(BlendMode mode, bool enabled) {
	gl_renderer_->SetBlend(mode, enabled);
}

void Renderer::SetDepth(const DepthState& depth) {
	gl_renderer_->SetDepth(depth);
}

void Renderer::SetStencil(const StencilState& stencil) {
	gl_renderer_->SetStencil(stencil);
}

void Renderer::SetRaster(const RasterState& raster) {
	gl_renderer_->SetRaster(raster);
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	gl_renderer_->SetColorMask(color_mask);
}

impl::RenderPass Renderer::BeginPass(const impl::RenderTargetData& scene_target) {
	return gl_renderer_->BeginPass(scene_target);
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

void Renderer::BeginFrame() {
	gl_renderer_->BeginFrame(window_.GetSize());
}

void Renderer::EndFrame() {
	gl_renderer_->EndFrame(display_viewport_);
}

void Renderer::UpdateDisplayViewport(V2_int window_size, bool emit_events) {
	PTGN_ASSERT(window_size == window_.GetSize());

	auto game_size{ game_size_.value_or(window_size) };

	PTGN_ASSERT(window_size.BothAboveZero());
	PTGN_ASSERT(game_size.BothAboveZero());

	Viewport viewport{ .position = { 0, 0 }, .size = window_size };

	auto compute_aspect_fit = [&viewport, game_size, window_size](bool letterbox_mode) {
		float window_aspect{ static_cast<float>(window_size.x) / window_size.y };
		float game_aspect{ static_cast<float>(game_size.x) / game_size.y };

		// In letterbox mode we need require window_aspect > game_aspect to fit height, and in
		// overscan we require window_aspect > game_aspect to fit height.
		bool fit_height{ (window_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			viewport.size.y = window_size.y;
			viewport.size.x =
				static_cast<int>(static_cast<float>(window_size.y) * game_aspect + 0.5f);
			viewport.position.x = (window_size.x - viewport.size.x) / 2; // left edge.
			viewport.position.y = 0;
		} else {
			// Fit width.
			viewport.size.x = window_size.x;
			viewport.size.y =
				static_cast<int>(static_cast<float>(window_size.x) / game_aspect + 0.5f);
			viewport.position.x = 0;
			viewport.position.y = (window_size.y - viewport.size.y) / 2; // top edge.
		}
	};

	switch (scaling_mode_) {
		case ScalingMode::Letterbox: compute_aspect_fit(true); break;
		case ScalingMode::Overscan:	 compute_aspect_fit(false); break;

		case ScalingMode::Stretch:
			PTGN_ASSERT(viewport.position == V2_int{});
			PTGN_ASSERT(viewport.size == window_.GetSize());
			// Viewport is full window (default).
			break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ window_size / game_size };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			viewport.size	  = game_size * scale;				   // scale up.
			viewport.position = (window_size - viewport.size) / 2; // center of window.
			break;
		}

		case ScalingMode::Disabled:
			viewport.size	  = game_size;						   // no change.
			viewport.position = (window_size - viewport.size) / 2; // center of window.
			break;

		default: PTGN_ERROR("Unsupported resolution mode");
	}

	bool resized{ viewport.size != display_viewport_.size };
	bool moved{ viewport.position != display_viewport_.position };

	if (resized || moved) {
		PTGN_ASSERT(viewport.size.BothAboveZero());

		display_viewport_ = viewport;

		if (resized) {
			gl_renderer_->ResizeScreenTarget(display_viewport_.size);

			if (emit_events) {
				impl::DisplayResized display_resized;
				display_resized.size = display_viewport_.size;
				// PTGN_LOG("Emitting display resized: ", display_resized.size);
				events_.Emit(display_resized);
			}
		}

		if (emit_events) {
			impl::DisplayViewportChanged display_changed;
			display_changed.viewport = display_viewport_;
			// PTGN_LOG("Emitting viewport changed: ", display_changed.viewport);
			events_.Emit(display_changed);
		}
	}
}

} // namespace ptgn

// TODO: Fix.
// template <ShapeType T>
// static void DrawShape(Renderer& renderer, Entity entity) {
//	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());
//
//	Origin origin{ Origin::Center };
//
//	if constexpr (IsAnyOf<T, Rect, RoundedRect>) {
//		origin = GetDrawOrigin(entity);
//	}
//
//	const auto& shape{ entity.Get<T>() };
//
//	renderer.DrawShape(
//		GetDrawTransform(entity), shape, GetTint(entity), entity.GetOrDefault<LineWidth>(), origin,
//		GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
//		entity.GetOrDefault<PostFX>(), entity.GetOrDefault<ShaderPass>()
//	);
//}
// void Renderer::DrawLightQuad(const LightParams& light) {
//	QuadParams p{};
//	p.center = light.position;
//	p.size	 = { light.radius * 2.0f, light.radius * 2.0f };
//	p.tint	 = color::White;
//
//	auto shader = gl_->GetShader("light");
//
//	DrawQuadEx(shader, p, [&](const auto& s, auto&) {
//		gl_->SetUniform(s, "u_LightPosition", light.position);
//		gl_->SetUniform(s, "u_Color", light.color.Normalized());
//		gl_->SetUniform(s, "u_LightIntensity", light.intensity);
//		gl_->SetUniform(s, "u_LightRadius", light.radius);
//		gl_->SetUniform(s, "u_Falloff", light.falloff);
//		gl_->SetUniform(s, "u_AmbientColor", light.ambient_color);
//		gl_->SetUniform(s, "u_AmbientIntensity", light.ambient_intensity);
//		gl_->SetUniform(s, "u_LightAttenuation", light.attenuation);
//	});
// }
//*/
//
// static float GetFade(float diameter_y) {
//	constexpr float fade_scaling_constant{ 0.12f };
//	return fade_scaling_constant / diameter_y;
// }
//
// static float GetFade(V2_float diameter) {
//	return GetFade(diameter.y);
// }
//
// static float NormalizeArcLineWidthToThickness(float line_width, float fade, V2_float radii) {
//	if (line_width == -1.0f) {
//		// Internally line width for a filled SDF is 1.0f.
//		line_width = 1.0f;
//	} else {
//		PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for circle");
//
//		// Internally line width for a completely hollow ellipse is 0.0f.
//		line_width = fade + line_width / std::min(radii.x, radii.y);
//	}
//	return line_width;
// }
//
// static float GetAspectRatio(V2_float size) {
//	PTGN_ASSERT(size.x > 0.0f);
//	return size.y / size.x;
// }
//
// static float GetNormalizedRadius(float diameter, float size_x) {
//	PTGN_ASSERT(size_x > 0.0f);
//	float normalized_radius{ diameter / size_x };
//	return std::clamp(normalized_radius, 0.0f, 1.0f);
// }
//
// template <ShapeType T>
// static std::array<float, 4> GetData(
//	const T& shape, auto radius, float line_width, V2_float size
//) {
//	std::array<float, 4> data{ 0.0f, 0.0f, 0.0f, 0.0f };
//
//	auto diameter{ 2.0f * radius };
//
//	float fade{ GetFade(diameter) };
//
//	float thickness{ NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius }) };
//
//	data[0] = thickness;
//	data[1] = fade;
//
//	if constexpr (std::is_same_v<T, Arc>) {
//		float aperture{ shape.GetAperture() };
//		float direction{ shape.clockwise ? 1.0f : -1.0f };
//
//		data[2] = aperture;
//		data[3] = direction;
//	} else if constexpr (IsAnyOf<T, Capsule, RoundedRect>) {
//		float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
//		float aspect_ratio{ GetAspectRatio(size) };
//
//		data[2] = normalized_radius;
//		data[3] = aspect_ratio;
//	}
//
//	return data;
// }
//
// struct QuadInfo {
//	std::array<V2_float, 4> points;
//	std::array<float, 4> data{ 0.0f, 0.0f, 0.0f, 0.0f };
// };
//
// template <ShapeType T>
// static std::optional<QuadInfo> GetQuadInfo(Renderer& ctx, DrawShapeCommand& cmd, const T& shape)
// { 	QuadInfo info;
//
//	const auto set_shader = [](DrawShapeCommand& c, std::string_view shader_name) {
//		if (c.render_state.shader_pass.has_value() && *c.render_state.shader_pass != ShaderPass{}) {
//			return;
//		}
//		c.render_state.shader_pass = Application::Get().shader.Get(shader_name);
//	};
//
//	if constexpr (std::is_same_v<T, V2_float>) {
//		Transform translated = cmd.transform;
//		translated.Translate(shape);
//
//		Rect r{ V2_float{ 1.0f } };
//
//		info.points = r.GetWorldVertices(translated, Origin::Center);
//	} else if constexpr (std::is_same_v<T, Line>) {
//		if (cmd.line_width < min_line_width) {
//			return std::nullopt;
//		}
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.line_width);
//	} else if constexpr (std::is_same_v<T, Capsule>) {
//		auto radius{ shape.GetRadius(cmd.transform) };
//
//		if (radius <= 0.0f) {
//			return std::nullopt;
//		}
//
//		V2_float size;
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform, &size);
//		info.data	= GetData(shape, radius, cmd.line_width, size);
//
//		set_shader(cmd, "capsule");
//	} else if constexpr (std::is_same_v<T, Arc>) {
//		auto radius{ shape.GetRadius(cmd.transform) };
//
//		if (radius <= 0.0f) {
//			return std::nullopt;
//		}
//
//		Transform rotated{ cmd.transform };
//		rotated.Rotate(shape.GetStartAngle());
//
//		info.points = shape.GetWorldQuadVertices(rotated);
//		info.data	= GetData(shape, radius, cmd.line_width, {});
//
//		set_shader(cmd, "arc");
//	} else if constexpr (std::is_same_v<T, RoundedRect>) {
//		auto size = shape.GetSize(cmd.transform);
//
//		if (!size.BothAboveZero()) {
//			return std::nullopt;
//		}
//
//		float radius = shape.GetRadius(cmd.transform);
//
//		if (radius <= 0.0f) {
//			cmd.render_state.shader_pass = std::nullopt;
//			cmd.shape					 = Rect{ shape.GetSize() };
//			ctx.DrawCommand(cmd);
//			return std::nullopt;
//		}
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.origin);
//		info.data	= GetData(shape, radius, cmd.line_width, size);
//
//		set_shader(cmd, "rounded_rect");
//	} else if constexpr (std::is_same_v<T, Ellipse>) {
//		auto radius = shape.GetRadius(cmd.transform);
//
//		if (!radius.BothAboveZero()) {
//			return std::nullopt;
//		}
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform);
//		info.data	= GetData(shape, radius, cmd.line_width, {});
//
//		set_shader(cmd, "circle");
//	} else {
//		return std::nullopt;
//	}
//
//	return info;
// }
//
// template <ShapeType T>
// static void DrawShape(Renderer& ctx, DrawShapeCommand cmd, const T& shape) {
//	if constexpr (IsAnyOf<T, V2_float, Line, Capsule, Arc, RoundedRect, Ellipse>) {
//		auto info{ GetQuadInfo(ctx, cmd, shape) };
//
//		if (!info.has_value()) {
//			return;
//		}
//
//		const auto& [points, data] = *info;
//
//		auto quad_vertices{
//			Vertex::GetQuad(points, cmd.tint, cmd.depth, data, GetDefaultTextureCoordinates())
//		};
//
//		ctx.SetState(cmd.render_state);
//		ctx.AddVertices(quad_vertices, quad_indices);
//	} else if constexpr (std::is_same_v<T, Circle>) {
//		cmd.shape = Ellipse{ V2_float{ shape.GetRadius() } };
//		ctx.DrawCommand(cmd);
//	} else if constexpr (std::is_same_v<T, Rect>) {
//		if (auto size{ shape.GetSize(cmd.transform) }; !size.BothAboveZero()) {
//			return;
//		}
//
//		auto points = shape.GetWorldVertices(cmd.transform, cmd.origin);
//		auto vertices =
//			Vertex::GetQuad(points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());
//
//		ctx.SetState(cmd.render_state);
//
//		if (cmd.line_width == -1.0f) {
//			ctx.AddVertices(vertices, quad_indices);
//		} else {
//			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
//		}
//
//	} else if constexpr (std::is_same_v<T, Triangle>) {
//		auto points	  = shape.GetWorldVertices(cmd.transform);
//		auto vertices = Vertex::GetTriangle(points, cmd.tint, cmd.depth);
//
//		ctx.SetState(cmd.render_state);
//
//		if (cmd.line_width == -1.0f) {
//			ctx.AddVertices(vertices, triangle_indices);
//		} else {
//			ctx.AddLinesImpl(vertices, triangle_indices, points, cmd.line_width, {});
//		}
//	} else if constexpr (std::is_same_v<T, Polygon>) {
//		ctx.SetState(cmd.render_state);
//
//		if (shape.vertices.size() < 3) {
//			if (shape.vertices.empty()) {
//				return;
//			} else if (shape.vertices.size() == 1) {
//				cmd.shape = V2_float{ shape.vertices.front() };
//				ctx.DrawCommand(cmd);
//				return;
//			} else if (shape.vertices.size() == 2) {
//				cmd.shape = Line{ shape.vertices[0], shape.vertices[1] };
//				ctx.DrawCommand(cmd);
//				return;
//			}
//		}
//
//		auto points = shape.GetWorldVertices(cmd.transform);
//
//		if (cmd.line_width == -1.0f) {
//			auto triangles{ Triangulate(points) };
//			for (const auto& triangle : triangles) {
//				auto vertices = Vertex::GetTriangle(triangle, cmd.tint, cmd.depth);
//				ctx.AddVertices(vertices, triangle_indices);
//			}
//		} else {
//			auto vertices =
//				Vertex::GetQuad({}, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());
//			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
//		}
//	}
// }
//
// void Renderer::DrawLines(const DrawLinesCommand& cmd) {
//	std::size_t count = cmd.points.size();
//
//	PTGN_ASSERT(cmd.line_width >= min_line_width);
//
//	PTGN_ASSERT(
//		(cmd.connect_last_to_first && count >= 3) || (!cmd.connect_last_to_first && count >= 2)
//	);
//
//	std::size_t vertex_modulo = count;
//	if (!cmd.connect_last_to_first) {
//		vertex_modulo -= 1;
//	}
//
//	SetState(cmd.render_state);
//
//	for (std::size_t i = 0; i < count; ++i) {
//		Line l{ cmd.points[i], cmd.points[(i + 1) % vertex_modulo] };
//		auto quad_points   = l.GetWorldQuadVertices(cmd.transform, cmd.line_width);
//		auto quad_vertices = Vertex::GetQuad(
//			quad_points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates()
//		);
//		AddVertices(quad_vertices, quad_indices);
//	}
// }
//
// void Renderer::AddTemporaryTexture(Texture&& texture) {
//	temporary_textures.emplace_back(std::move(texture));
// }
//
// void Renderer::AddLinesImpl(
//	std::span<Vertex> line_vertices, std::span<const Index> line_indices,
//	std::span<const V2_float> points, float line_width, const Transform& transform
//) {
//	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for lines");
//
//	for (std::size_t i = 0; i < points.size(); ++i) {
//		Line l{ points[i], points[(i + 1) % points.size()] };
//		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };
//
//		PTGN_ASSERT(line_vertices.size() <= line_points.size());
//
//		for (std::size_t j = 0; j < line_vertices.size(); ++j) {
//			line_vertices[j].position[0] = line_points[j].x;
//			line_vertices[j].position[1] = line_points[j].y;
//		}
//
//		AddVertices(line_vertices, line_indices);
//	}
// }
//