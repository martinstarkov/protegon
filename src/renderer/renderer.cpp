#include "renderer/renderer.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/effects.h"
#include "components/generic.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "debug/log.h"
#include "math/geometry.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/font.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

namespace ptgn {

namespace impl {

const Depth max_depth{ std::numeric_limits<std::int32_t>::max() };
const BlendMode debug_blend_mode{ BlendMode::Blend };

RenderState GetDebugRenderState(const Camera& camera) {
	impl::RenderState state;
	state.blend_mode  = debug_blend_mode;
	state.camera	  = camera;
	state.shader_pass = { game.shader.Get<ShapeShader::Quad>() };
	state.post_fx	  = {};
	return state;
}

} // namespace impl

void DrawDebugTexture(
	const TextureHandle& texture_key, const V2_float& position, const V2_float& size, Origin origin,
	float rotation, const Camera& camera
) {
	game.renderer.GetRenderData().AddTexturedQuad(
		texture_key.GetTexture(), Transform{ position, rotation },
		size.IsZero() ? V2_float{ texture_key.GetSize() } : size, origin, color::White,
		impl::max_depth, impl::default_texture_coordinates, impl::GetDebugRenderState(camera), {}
	);
}

void DrawDebugText(
	const std::string& content, const V2_float& position, const TextColor& color, Origin origin,
	const FontSize& font_size, bool hd_text, const ResourceHandle& font_key,
	const TextProperties& properties, const V2_float& size, float rotation, const Camera& camera
) {
	auto& render_data{ game.renderer.GetRenderData() };
	FontSize final_font_size{ font_size };
	Transform transform{ position, rotation };
	if (hd_text) {
		const auto& scene{ game.scene.GetCurrent() };
		auto scene_scale{ scene.GetScale(camera) };
		transform.Scale(1.0f / scene_scale);
		final_font_size = static_cast<std::int32_t>(static_cast<float>(font_size) * scene_scale.y);
	}
	auto texture{ Text::CreateTexture(content, color, final_font_size, font_key, properties) };
	render_data.AddTexturedQuad(
		texture, transform,
		size.IsZero() ? V2_float{ Text::GetSize(content, font_key, final_font_size) } : size,
		origin, color::White, impl::max_depth, impl::GetDefaultTextureCoordinates(),
		impl::GetDebugRenderState(camera), {}
	);
	render_data.AddTemporaryTexture(std::move(texture));
}

void DrawDebugLine(
	const V2_float& line_start, const V2_float& line_end, const Color& color, float line_width,
	const Camera& camera
) {
	game.renderer.GetRenderData().AddLine(
		line_start, line_end, color, impl::max_depth, line_width, impl::GetDebugRenderState(camera)
	);
}

void DrawDebugLines(
	const std::vector<V2_float>& points, const Color& color, float line_width,
	bool connect_last_to_first, const Camera& camera
) {
	game.renderer.GetRenderData().AddLines(
		points, color, impl::max_depth, line_width, connect_last_to_first,
		impl::GetDebugRenderState(camera)
	);
}

void DrawDebugTriangle(
	const std::array<V2_float, 3>& vertices, const Color& color, float line_width,
	const Camera& camera
) {
	game.renderer.GetRenderData().AddTriangle(
		vertices, color, impl::max_depth, line_width, impl::GetDebugRenderState(camera)
	);
}

void DrawDebugRect(
	const V2_float& position, const V2_float& size, const Color& color, Origin origin,
	float line_width, float rotation, const Camera& camera
) {
	game.renderer.GetRenderData().AddQuad(
		Transform{ position, rotation }, size, origin, color, impl::max_depth, line_width,
		impl::GetDebugRenderState(camera)
	);
}

void DrawDebugEllipse(
	const V2_float& center, const V2_float& radii, const Color& color, float line_width,
	float rotation, const Camera& camera
) {
	auto state{ impl::GetDebugRenderState(camera) };
	state.shader_pass = game.shader.Get<ShapeShader::Circle>();

	game.renderer.GetRenderData().AddEllipse(
		Transform{ center, rotation }, radii, color, impl::max_depth, line_width, state
	);
}

void DrawDebugCircle(
	const V2_float& center, float radius, const Color& color, float line_width, const Camera& camera
) {
	auto state{ impl::GetDebugRenderState(camera) };
	state.shader_pass = game.shader.Get<ShapeShader::Circle>();

	game.renderer.GetRenderData().AddCircle(
		Transform{ center }, radius, color, impl::max_depth, line_width, state
	);
}

void DrawDebugCapsule(
	const V2_float& start, const V2_float& end, float radius, const Color& color, float line_width,
	const Camera& camera
) {
	auto state{ impl::GetDebugRenderState(camera) };

	auto& render_data{ game.renderer.GetRenderData() };

	// TODO: Fix and replace with game.renderer.GetRenderData().AddCapsule when capsule shader is
	// implemented.
	render_data.AddCircle(Transform{ start }, radius, color, impl::max_depth, line_width, state);
	render_data.AddCircle(Transform{ end }, radius, color, impl::max_depth, line_width, state);
	render_data.AddLine(start, end, color, impl::max_depth, line_width, state);
}

void DrawDebugPolygon(
	const std::vector<V2_float>& vertices, const Color& color, float line_width,
	const Camera& camera
) {
	game.renderer.GetRenderData().AddPolygon(
		vertices, color, impl::max_depth, line_width, impl::GetDebugRenderState(camera)
	);
}

void DrawDebugPoint(const V2_float position, const Color& color, const Camera& camera) {
	game.renderer.GetRenderData().AddPoint(
		position, color, impl::max_depth, impl::GetDebugRenderState(camera)
	);
}

void DrawDebugShape(
	const Transform& transform, const Shape& shape, const Color& color, float line_width,
	const Camera& camera
) {
	std::visit(
		[&](const auto& s1) {
			using S1 = std::decay_t<decltype(s1)>;
			if constexpr (std::is_same_v<S1, Point>) {
				DrawDebugPoint(s1, color, camera);
			} else if constexpr (std::is_same_v<S1, Rect>) {
				DrawDebugRect(
					transform.GetPosition(), s1.GetSize(transform), color, Origin::Center,
					line_width, transform.GetRotation(), camera
				);
			} else if constexpr (std::is_same_v<S1, Circle>) {
				DrawDebugCircle(
					transform.GetPosition(), s1.GetRadius(transform), color, line_width, camera
				);
			} else if constexpr (std::is_same_v<S1, Line>) {
				auto [start, end] = s1.GetWorldVertices(transform);
				DrawDebugLine(start, end, color, line_width, camera);
			} else if constexpr (std::is_same_v<S1, Triangle>) {
				auto v = s1.GetWorldVertices(transform);
				DrawDebugTriangle(v, color, line_width, camera);
			} else if constexpr (std::is_same_v<S1, Polygon>) {
				auto v = s1.GetWorldVertices(transform);
				DrawDebugPolygon(v, color, line_width, camera);
			} else if constexpr (std::is_same_v<S1, Capsule>) {
				auto [start, end] = s1.GetWorldVertices(transform);
				DrawDebugCapsule(start, end, s1.GetRadius(transform), color, line_width, camera);
			} else {
				PTGN_ERROR("Cannot draw unknown shape type");
			}
		},
		shape
	);
}

namespace impl {

void Renderer::Init() {
	render_data_.Init();
}

void Renderer::SetBackgroundColor(const Color& background_color) {
	render_data_.screen_target_.SetClearColor(background_color);
}

Color Renderer::GetBackgroundColor() const {
	return render_data_.screen_target_.GetClearColor();
}

void Renderer::Reset() {
	bound_ = {};

	FrameBuffer::Unbind(); // Will set bound_frame_buffer_id_ to 0.

	render_data_ = {};
	render_data_.Init();
}

void Renderer::Shutdown() {
	Reset();
}

void Renderer::SetLogicalResolutionMode(LogicalResolutionMode logical_resolution_mode) {
	V2_int resolution{ render_data_.logical_resolution_set_ ? render_data_.logical_resolution_
															: game.window.GetSize() };
	render_data_.UpdateResolutions(resolution, logical_resolution_mode);
}

void Renderer::SetLogicalResolution(
	const V2_int& logical_resolution, LogicalResolutionMode logical_resolution_mode
) {
	render_data_.logical_resolution_set_ = !logical_resolution.IsZero();
	V2_int resolution{ render_data_.logical_resolution_set_ ? logical_resolution
															: game.window.GetSize() };
	render_data_.UpdateResolutions(resolution, logical_resolution_mode);
}

V2_int Renderer::GetPhysicalResolution() const {
	return render_data_.physical_viewport_.size;
}

V2_int Renderer::GetLogicalResolution() const {
	return render_data_.logical_resolution_;
}

LogicalResolutionMode Renderer::GetLogicalResolutionMode() const {
	return render_data_.resolution_mode_;
}

RenderData& Renderer::GetRenderData() {
	return render_data_;
}

void Renderer::PresentScreen() {
	FrameBuffer::Unbind();

	// PTGN_ASSERT(
	// 	std::invoke([]() {
	// 		auto viewport_size{ GLRenderer::GetViewportSize() };
	// 		if (viewport_size.IsZero()) {
	// 			return false;
	// 		}
	// 		if (viewport_size.x == 1 && viewport_size.y == 1) {
	// 			return false;
	// 		}
	// 		return true;
	// 	}),
	// 	"Attempting to render to 0 or 1 sized viewport"
	// );

	PTGN_ASSERT(
		FrameBuffer::IsUnbound(),
		"Frame buffer must be unbound (id=0) before swapping SDL2 buffer to the screen"
	);

	game.window.SwapBuffers();
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::SetClearColor(color::Transparent);
	GLRenderer::Clear();
	render_data_.ClearScreenTarget();
}

} // namespace impl

} // namespace ptgn