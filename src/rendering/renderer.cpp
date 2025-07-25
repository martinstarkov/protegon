#include "rendering/renderer.h"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/text.h"
#include "resources/render_target.h"
#include "resources/texture.h"
#include "scene/camera.h"

// TODO: Fix.
// TODO: Fix all the debug functions.

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
	const FontSize& font_size, const FontKey& font_key, const TextProperties& properties,
	const V2_float& size, float rotation, const Camera& camera
) {
	auto& render_data{ game.renderer.GetRenderData() };
	auto texture{ Text::CreateTexture(content, color, font_size, font_key, properties) };
	render_data.AddTexturedQuad(
		texture, Transform{ position, rotation },
		size.IsZero() ? V2_float{ Text::GetSize(content, font_key) } : size, origin, color::White,
		impl::max_depth, impl::GetDefaultTextureCoordinates(), impl::GetDebugRenderState(camera), {}
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

namespace impl {

void GetRenderArea(
	const V2_float& screen_size, const V2_float& target_size, ResolutionMode mode,
	V2_float& out_position, V2_float& out_size
) {
	switch (mode) {
		case ResolutionMode::Disabled:
		case ResolutionMode::Stretch:
			out_position = {};
			out_size	 = screen_size;
			break;

		case ResolutionMode::Letterbox: {
			PTGN_ASSERT(!target_size.IsZero());
			auto ratio{ screen_size / target_size };
			float scale	 = std::min(ratio.x, ratio.y);
			out_size	 = V2_float{ target_size } * scale;
			out_position = (V2_float{ screen_size } - out_size) / 2.0f;
			break;
		}

		case ResolutionMode::Overscan: {
			PTGN_ASSERT(!target_size.IsZero());
			auto ratio{ screen_size / target_size };
			float scale	 = std::max(ratio.x, ratio.y);
			out_size	 = V2_float{ target_size } * scale;
			out_position = (V2_float{ screen_size } - out_size) / 2.0f;
			break;
		}

		case ResolutionMode::IntegerScale: {
			PTGN_ASSERT(!target_size.IsZero());
			auto ratio{ screen_size / target_size };
			int scale	 = static_cast<int>(std::min(ratio.x, ratio.y));
			scale		 = std::max(1, scale); // avoid zero
			out_size	 = V2_float{ target_size } * static_cast<float>(scale);
			out_position = (V2_float{ screen_size } - out_size) / 2.0f;
			break;
		}
		default: PTGN_ERROR("Unsupported resolution mode");
	}
}

void Renderer::Init() {
	render_data_.Init();
}

void Renderer::SetBackgroundColor(const Color& background_color) {
	render_data_.screen_target.SetClearColor(background_color);
}

Color Renderer::GetBackgroundColor() const {
	return render_data_.screen_target.GetClearColor();
}

void Renderer::Reset() {
	resolution_	  = {};
	scaling_mode_ = ResolutionMode::Disabled;

	bound_ = {};

	FrameBuffer::Unbind(); // Will set bound_frame_buffer_id_ to 0.
}

void Renderer::Shutdown() {
	Reset();
}

void Renderer::SetResolution(const V2_int& resolution) {
	resolution_ = resolution;
	// User expects setting resolution to take effect immediately so it is defaulted to stretch.
	if (scaling_mode_ == ResolutionMode::Disabled) {
		scaling_mode_ = ResolutionMode::Stretch;
	}
}

void Renderer::SetResolutionMode(ResolutionMode scaling_mode) {
	scaling_mode_ = scaling_mode;
}

V2_int Renderer::GetResolution() const {
	if (resolution_.IsZero()) {
		return game.window.GetSize();
	}
	return resolution_;
}

ResolutionMode Renderer::GetResolutionMode() const {
	return scaling_mode_;
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

	// TODO: Fix.
	/*
	// TODO: Move this to happen only when setting resolution. This would allow for example only one
	// render target to be drawn as resolution.
	auto camera{ screen_target_.GetCamera().GetPrimary() };
	Rect dest{ Rect::Fullscreen() };
	auto center_on_resolution = [&]() {
		camera.CenterOnArea(resolution_.IsZero() ? game.window.GetSize() : resolution_);
	};
	std::function<void()> post_flush;
	switch (scaling_mode_) {
		case ResolutionMode::Disabled:
			camera.SetToWindow();
			// Uses fullscreen.
			// resolution_ = {};
			break;
		case ResolutionMode::Stretch:
			std::invoke(center_on_resolution);
			//   resolution_ = {};
			//   resolution_.origin = Origin::TopLeft;
			//   resolution_.size = resolution;
			break;
		case ResolutionMode::Letterbox: {
			// Size of the blackbars on one side.
			V2_float letterbox_size{ 160, 0 };
			V2_float size{ resolution_.IsZero() ? game.window.GetSize() : resolution_ };
			std::invoke(center_on_resolution);
			// camera.SetSize(size + letterbox_size);
			//  camera.SetPosition(size / 2.0f);
			GLRenderer::SetViewport(letterbox_size, game.window.GetSize() - 2.0f * letterbox_size);
			break;
		}
		case ResolutionMode::Overscan:	   break;
		case ResolutionMode::IntegerScale: break;
		default:						   PTGN_ERROR("Unrecognized resolution mode");
	}
	*/
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::SetClearColor(color::Transparent);
	GLRenderer::Clear();
}

} // namespace impl

} // namespace ptgn