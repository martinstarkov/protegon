#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/hash.h"
#include "platform/window.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/vertex/vertex.h"

namespace ptgn::impl {

Renderer::Renderer(Window& window, EventSink&& event_sink) :
	window_{ window },
	event_sink_{ std::move(event_sink) },
	gl_{ std::make_unique<gl::GLContext>() },
	batcher_{ *this },
	target_pool_{ *this },
	pipeline_manager_{ *this } {
	pipeline_manager_.AddPipeline<TextureVertex>(
		"texture", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<ShapeVertex>(
		"shape", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<ColorVertex>(
		"color", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);

	SetCurrentPipeline("texture");

	game_size_ = GetFullViewportSize();

	auto display{ RecalculateDisplayViewport() };

	display_viewport_		= display.viewport;
	display_viewport_dirty_ = false;

	auto display_size{ GetDisplaySize() };

	PTGN_ASSERT(display_size.BothAboveZero(), "Display size cannot be zero");

	screen_target_ = CreateRenderTarget({ .size{ display_size }, .format{ TextureFormat::RGBA8 } });
	BindScreenTarget();
	SetViewProjection(display_size);

	auto max_texture_slots{ GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto text{ gl_->shaders.GetProgram("text") };
	auto _2 = gl_->Bind(text, false);
	SetUniform(text, "u_Textures", samplers);

	auto quad{ gl_->shaders.GetProgram("texture") };
	auto _1 = gl_->Bind(quad, false);
	SetUniform(quad, "u_Textures", samplers);

#ifdef PTGN_PLATFORM_MACOS
	//  Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	//  GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	//  texture because texture unloadable."
	for (auto i{ 0u }; i < max_texture_slots; ++i) {
		gl_->SetActiveTextureSlot(i);
		auto _3 = gl_->Bind(TextureId{ 0 }, false);
	}
#endif
}

Renderer::~Renderer() noexcept {
	// Guarantees that a vertex array object is bound before destroying any buffers.
	auto _{ gl_->Bind(VertexArrayId{ 0 }, false) };
}

void Renderer::FlushBatch() {
	batcher_.Flush();
}

RenderPipeline& Renderer::GetPipeline(PipelineId id) {
	return pipeline_manager_.GetPipeline(id);
}

const RenderPipeline& Renderer::GetPipeline(PipelineId id) const {
	return pipeline_manager_.GetPipeline(id);
}

RenderTargetObject Renderer::CreateRenderTarget(const RenderTargetDesc& desc) {
	PTGN_ASSERT(desc.size.BothAboveZero(), "Cannot create render target with zero size");

	auto color = gl_->textures.CreateTexture(desc.size, desc.format, desc.params);

	std::optional<RenderbufferId> depth;

	if (!IsColorFormat(desc.format)) {
		depth = gl_->renderbuffers.CreateRenderbuffer(desc.size, desc.format);
	}

	using enum gl::Attachment;

	auto framebuffer = gl_->framebuffers.CreateFramebuffer(
		color, Color0, depth, IsDepthOnlyFormat(desc.format) ? Depth : DepthStencil
	);

	return RenderTargetObject{ this, RenderTargetId{ framebuffer } };
}

TextureId Renderer::GetRenderTargetTexture(RenderTargetId render_target) const {
	const auto& color_attachment{ gl_->framebuffers.GetFramebufferAttachment(
		FramebufferId{ render_target }, gl::Attachment::Color0
	) };
	PTGN_ASSERT(
		color_attachment.id,
		"Render target must have a valid color attachment for its texture to be retrieved"
	);
	return TextureId{ color_attachment.id };
}

V2_int Renderer::GetRenderTargetSize(RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto size{ gl_->textures.GetTextureSize(id) };

	return size;
}

TextureFormat Renderer::GetRenderTargetTextureFormat(RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto texture_format{ gl_->textures.GetTextureFormat(id) };

	return texture_format;
}

void Renderer::ClearRenderTarget(
	RenderTargetId render_target, Color color, bool set_viewport, bool restore_bind
) const {
	auto bind_guard = gl_->Bind(FramebufferId{ render_target }, restore_bind);

	std::optional<Viewport> viewport;
	if (set_viewport) {
		viewport = gl_->GetViewport();

		auto render_target_size{ GetRenderTargetSize(render_target) };

		gl_->SetViewport({ .position{}, .size{ render_target_size } });
	}

	gl_->framebuffers.ClearToColor(FramebufferId{ render_target }, color);

	if (set_viewport && viewport.has_value()) {
		gl_->SetViewport(*viewport);
	}
}

void Renderer::SetCurrentPipeline(std::string_view name) {
	SetCurrentPipeline(Hash(name));
}

void Renderer::SetCurrentPipeline(std::size_t id) {
	if (pipeline_manager_.IsCurrentPipeline(id)) {
		return;
	}
	PTGN_ASSERT(pipeline_manager_.HasPipeline(id), "No matching pipeline found: ", id);
	FlushBatch();
	pipeline_manager_.SetCurrentPipeline(id);
}

void Renderer::SetViewport(Viewport viewport) {
	if (viewport == gl_->GetViewport()) {
		return;
	}
	FlushBatch();
	gl_->SetViewport(viewport);
}

void Renderer::SetShader(ShaderId shader) {
	if (shader == gl_->GetBoundState().shader_program) {
		return;
	}
	FlushBatch();
	auto _ = gl_->Bind(shader, false);
	gl_->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	if (blend_mode == gl_->GetBoundState().blend_mode) {
		return;
	}
	FlushBatch();
	gl_->SetBlendMode(blend_mode);
}

PipelineId Renderer::GetTexturePipeline() const {
	return Hash("texture");
}

void Renderer::SetRenderTarget(const RenderTargetObject* target) {
	if (target) {
		SetFramebuffer(FramebufferId{ target->operator RenderTargetId() });
	}
	current_target_ = target;
}

void Renderer::SetViewProjection(V2_float size) {
	SetViewProjection(Matrix4::Orthographic(size));
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ != view_projection) {
		FlushBatch();
		view_projection_ = view_projection;
	}
	// TODO: Find a better way to do this. This is needed to ensure that the shader's
	// uniform is updated even if the shader itself doesn't change.
	if (auto shader{ gl_->GetBoundShader() }; shader.has_value() && *shader) {
		gl_->shaders.SetUniform(*shader, "u_ViewProjection", view_projection_);
	}
}

void Renderer::SetFramebuffer(FramebufferId framebuffer) {
	if (framebuffer == gl_->GetBoundFramebuffer()) {
		return;
	}
	FlushBatch();
	auto _ = gl_->Bind(framebuffer, false);
}

void Renderer::SetDepthTesting(bool enabled) {
	if (enabled == gl_->GetBoundState().depth_testing) {
		return;
	}
	FlushBatch();
	gl_->SetDepthTesting(enabled);
}

void Renderer::SetDepthMask(const DepthMaskState& mask) {
	if (mask == gl_->GetBoundState().depth_mask) {
		return;
	}
	FlushBatch();
	gl_->SetDepthMask(mask);
}

void Renderer::SetStencil(const StencilState& stencil) {
	if (stencil == gl_->GetBoundState().stencil) {
		return;
	}
	FlushBatch();
	gl_->SetStencil(stencil);
}

void Renderer::SetRaster(const RasterState& raster) {
	if (raster == gl_->GetBoundState().raster) {
		return;
	}
	FlushBatch();
	gl_->SetRaster(raster);
}

void Renderer::SetScissor(const ScissorState& scissor) {
	if (scissor == gl_->GetBoundState().scissor) {
		return;
	}
	FlushBatch();
	gl_->SetScissor(scissor);
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	if (color_mask == gl_->GetBoundState().color_mask) {
		return;
	}
	FlushBatch();
	gl_->SetColorMask(color_mask);
}

ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_->shaders.GetProgram(name);
}

bool Renderer::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	auto bound{ gl_->GetBoundFramebuffer() };

	if (!bound.has_value() || *bound == FramebufferId{ 0 }) {
		return false;
	}

	return gl_->framebuffers.GetFramebufferAttachment(*bound, gl::Attachment::Color0).id == texture;
}

void Renderer::OnWindowResize(V2_int size) {
	if (presentation_viewport_.has_value()) {
		return;
	}

	if (!game_size_.has_value()) {
		event_sink_(size, ResizeType::Game);
	}

	event_sink_(size, impl::PresentationResizeType{});

	display_viewport_dirty_ = true;
}

void Renderer::SetGameSize(
	std::optional<V2_int> game_size, std::optional<ScalingMode> scaling_mode
) {
	if (game_size_ == game_size &&
		(!scaling_mode.has_value() || scaling_mode.has_value() && scaling_mode_ == scaling_mode)) {
		return;
	}

	PTGN_ASSERT(
		!game_size.has_value() || game_size.has_value() && game_size->BothAboveZero(),
		"Game size cannot be set to negative value or zero"
	);

	game_size_ = game_size;
	if (scaling_mode.has_value()) {
		scaling_mode_ = *scaling_mode;
	}

	auto size{ GetGameSize() };

	event_sink_(size, ResizeType::Game);

	display_viewport_dirty_ = true;
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	if (scaling_mode_ == scaling_mode) {
		return;
	}

	scaling_mode_ = scaling_mode;

	display_viewport_dirty_ = true;
}

void Renderer::SetPresentationViewport(std::optional<Viewport> presentation_viewport) {
	if (presentation_viewport_ == presentation_viewport) {
		return;
	}
	presentation_viewport_ = presentation_viewport;

	if (!presentation_viewport_.has_value()) {
		OnWindowResize(GetFullViewportSize());
		return;
	}

	if (!game_size_.has_value()) {
		event_sink_(presentation_viewport_->size, ResizeType::Game);
	}

	event_sink_(presentation_viewport_->size, impl::PresentationResizeType{});
	display_viewport_dirty_ = true;
}

bool Renderer::HasGameSize() const {
	return game_size_.has_value();
}

V2_int Renderer::GetGameSize() const {
	if (HasGameSize()) {
		return *game_size_;
	}
	return GetPresentationSize();
}

ScalingMode Renderer::GetScalingMode() const {
	return scaling_mode_;
}

Viewport Renderer::GetPresentationViewport() const {
	return { .position{ GetPresentationPosition() }, .size{ GetPresentationSize() } };
}

V2_int Renderer::GetPresentationPosition() const {
	if (presentation_viewport_.has_value()) {
		return presentation_viewport_->position;
	}
	return { 0, 0 };
}

V2_int Renderer::GetPresentationSize() const {
	if (presentation_viewport_.has_value()) {
		return presentation_viewport_->size;
	}
	return GetFullViewportSize();
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

V2_int Renderer::GetDisplayPosition() const {
	return display_viewport_.position;
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

V2_int Renderer::GetFullViewportSize() const {
	return window_.GetSize();
}

void Renderer::SetBackgroundColor(Color background_color) {
	background_color_ = background_color;
}

Color Renderer::GetBackgroundColor() const {
	return background_color_;
}

void Renderer::SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera) {
	primary_world_camera_ = primary_world_camera;
}

const std::optional<Camera>& Renderer::GetPrimaryWorldCamera() const {
	return primary_world_camera_;
}

void Renderer::UpdateDisplayViewport(bool emit_events) {
	if (!display_viewport_dirty_) {
		return;
	}

	auto resize_info = RecalculateDisplayViewport();

	display_viewport_dirty_ = false;

	if (!resize_info.moved && !resize_info.resized) {
		return;
	}

	display_viewport_ = resize_info.viewport;

	if (resize_info.resized) {
		ResizeScreenTarget(display_viewport_.size);

		if (emit_events) {
			event_sink_(display_viewport_.size, ResizeType::Display);
		}
	}
}

Renderer::DisplayResizeInfo Renderer::RecalculateDisplayViewport() const {
	const auto presentation{ GetPresentationViewport() };

	PTGN_ASSERT(presentation.size.BothAboveZero());

	auto game_size{ game_size_.value_or(presentation.size) };

	PTGN_ASSERT(game_size.BothAboveZero());

	Viewport viewport{ .position{}, .size{ presentation.size } };

	auto compute_aspect_fit = [&viewport, game_size, presentation](bool letterbox_mode) {
		float presentation_aspect{ static_cast<float>(presentation.size.x) / presentation.size.y };
		float game_aspect{ static_cast<float>(game_size.x) / game_size.y };

		// In letterbox mode we need require presentation_aspect > game_aspect to fit
		// height, and in overscan we require presentation_aspect > game_aspect to fit
		// height.
		bool fit_height{ (presentation_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			viewport.size.y = presentation.size.y;
			viewport.size.x =
				static_cast<int>(static_cast<float>(presentation.size.y) * game_aspect + 0.5f);
			viewport.position.x = (presentation.size.x - viewport.size.x) / 2; // left edge.
			viewport.position.y = 0;
		} else {
			// Fit width.
			viewport.size.x = presentation.size.x;
			viewport.size.y =
				static_cast<int>(static_cast<float>(presentation.size.x) / game_aspect + 0.5f);
			viewport.position.x = 0;
			viewport.position.y = (presentation.size.y - viewport.size.y) / 2; // top edge.
		}
	};

	switch (scaling_mode_) {
		case ScalingMode::Letterbox: compute_aspect_fit(true); break;
		case ScalingMode::Overscan:	 compute_aspect_fit(false); break;

		case ScalingMode::Stretch:
			PTGN_ASSERT(viewport.size == presentation.size);
			PTGN_ASSERT(viewport.position == V2_int{});
			// Viewport is full presentation area (default).
			break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ presentation.size / game_size };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			viewport.size = game_size * scale;			 // scale up.
			viewport.position =
				(presentation.size - viewport.size) / 2; // center of presentation viewport.
			break;
		}

		case ScalingMode::Disabled:
			viewport.size = game_size;					 // no change.
			viewport.position =
				(presentation.size - viewport.size) / 2; // center of presentation viewport.
			break;

		default: PTGN_ERROR("Unsupported resolution mode");
	}

	bool resized{ viewport.size != display_viewport_.size };
	bool moved{ viewport.position != display_viewport_.position };

	PTGN_ASSERT(viewport.size.BothAboveZero());

	return { .moved = moved, .resized = resized, .viewport{ viewport } };
}

void Renderer::ResizeScreenTarget(V2_int size) {
	ResizeRenderTarget(screen_target_.resource_, size);
}

void Renderer::BindScreenTarget() {
	SetRenderTarget(&screen_target_);
}

RenderTargetId Renderer::GetScreenTarget() const {
	return screen_target_.resource_;
}

void Renderer::InvalidateState() {
	gl_->InvalidateState();
}

void Renderer::BeginFrame() {
	InvalidateState();

	if (!presentation_viewport_.has_value()) {
		auto presentation{ GetPresentationViewport() };
		Color window_background_color{ window_.GetBackgroundColor() };

		auto _ = gl_->Bind(FramebufferId{ 0 }, false);
		gl_->SetClearColor(window_background_color);
		SetViewport(presentation);
		gl_->framebuffers.Clear();
	}

	screen_target_.Bind();
	screen_target_.Clear(background_color_, false, false);
}

void Renderer::EndFrame() {
	PTGN_ASSERT(display_viewport_.size.BothAboveZero());

	FlushBatch();

	SetRenderTarget(nullptr);
	SetFramebuffer(FramebufferId{ 0 });

	target_pool_.TrimUnused(0);

	if (presentation_viewport_.has_value()) {
		return;
	}

	SetViewport(display_viewport_);
	SetViewProjection(display_viewport_.size);
	SetBlendMode(BlendMode::ReplaceRGBA);

	PTGN_ASSERT(
		screen_target_.GetSize() == display_viewport_.size,
		"Screen target texture size must match display viewport size"
	);

	SetCurrentPipeline("texture");
	SetMaterial(
		MaterialState{
			.shader	  = GetShader("texture"),
			.uniforms = {},
		}
	);

	constexpr auto depth{ 0.0f };
	constexpr auto tint{ color::White };
	constexpr auto tex_coords{ GetDefaultTextureCoordinates<true>() };

	auto quad{
		CreateLocalRenderQuad(display_viewport_.size, depth, tint.Normalized(), tex_coords)
	};

	DrawTextureRequest request;
	request.quads	= { &quad, 1 };
	request.texture = screen_target_.GetTextureId();

	// TODO: Add screen texture effects.
	// request.effect_params = ...;

	DrawTexture(request);

	FlushBatch();
}

bool Renderer::IsPresentationViewportVisible() const {
	return presentation_viewport_.has_value() && !presentation_viewport_->size.BothAboveZero();
}

ShaderObject Renderer::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return ShaderObject{ this, gl_->shaders.CreateProgram(source, shader_name) };
}

TextureObject Renderer::CreateTexture(
	const Surface& surface, TextureFormat format, TextureParameters params
) {
	PTGN_ASSERT(
		surface.GetChannelCount() == GetChannelCount(format),
		"Surface and texture format channel count must match"
	);
	return CreateTexture(surface.Data(), surface.GetSize(), format, params);
}

TextureObject Renderer::CreateTexture(
	const std::uint8_t* pixel_data, V2_int size, TextureFormat format, TextureParameters params
) {
	auto [pixel_format, pixel_type] = gl::GetPixelDataFormat(format);
	PTGN_ASSERT(
		pixel_type == gl::PixelDataType::UnsignedByte, "Texture format must have a type of bytes"
	);
	return TextureObject{ this, gl_->textures.CreateTexture(
									pixel_data, pixel_format, pixel_type, size, format, params
								) };
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, const Matrix4& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V2_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V3_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V4_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, std::span<const float> v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V2_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V3_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V4_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, std::span<const int> v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, bool v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniformValue(ShaderId id, const char* uniform_name, const UniformValue& v) {
	std::visit([&]<typename T>(T&& s) { SetUniform(id, uniform_name, std::forward<T>(s)); }, v);
}

void Renderer::Destroy(VertexBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(ElementBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(UniformBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(ShaderId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(TextureId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(RenderbufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(FramebufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(VertexArrayId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(RenderTargetId id) {
	gl_->Destroy(id);
}

V2_int Renderer::GetTextureSize(TextureId texture) const {
	return gl_->textures.GetTextureSize(texture);
}

TextureFormat Renderer::GetTextureFormat(TextureId texture) const {
	return gl_->textures.GetTextureFormat(texture);
}

void Renderer::ResizeRenderTarget(RenderTargetId render_target, V2_int new_size) {
	gl_->framebuffers.ResizeFramebuffer(FramebufferId{ render_target }, new_size);
}

std::size_t Renderer::GetMaxTextureSlots() const {
	return gl_->GetMaxTextureSlots();
}

void Renderer::UploadVertices(const RenderPipeline& pipeline, std::span<const std::byte> vertices) {
	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _{ gl_->Bind(pipeline.vbo, false) };

	auto vertex_count{ static_cast<std::uint32_t>(vertices.size() / pipeline.vertex_size) };

	gl_->buffers.SetBufferSubData<VertexBufferId>(
		pipeline.vbo, gl::BufferTarget::ArrayBuffer, vertices.data(), 0, vertex_count,
		pipeline.vertex_size
	);
}

void Renderer::UploadIndices(const RenderPipeline& pipeline, std::span<const Index> indices) {
	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _{ gl_->Bind(pipeline.ebo, false) };

	gl_->buffers.SetBufferSubData<ElementBufferId>(
		pipeline.ebo, gl::BufferTarget::ElementArrayBuffer, indices.data(), 0,
		static_cast<std::uint32_t>(indices.size()), sizeof(Index)
	);
}

void Renderer::DrawElements(const RenderPipeline& pipeline, std::uint32_t index_count) {
	auto _0{ gl_->Bind(pipeline.vao, false) };

	gl_->vertex_arrays.DrawElements(
		pipeline.vao, index_count, gl::IndexType::UnsignedInt, pipeline.primitive_mode
	);
}

void Renderer::ApplyRenderTarget(RenderTargetId id) {
	auto _ = gl_->Bind(FramebufferId{ id }, false);
}

void Renderer::SetMaterial(const MaterialState& material) {
	SetShader(material.shader);

	if (current_uniforms_ == material.uniforms) {
		return;
	}
	FlushBatch();
	current_uniforms_ = material.uniforms;
}

RenderState Renderer::GetRenderState() const {
	const auto& state{ gl_->GetBoundState() };
	return RenderState{
		.viewport		 = gl_->GetViewport(),
		.view_projection = view_projection_,
		.blend_mode		 = state.blend_mode,
		.depth_testing	 = state.depth_testing,
		.depth_mask		 = state.depth_mask,
		.stencil		 = state.stencil,
		.raster			 = state.raster,
		.scissor		 = state.scissor,
		.color_mask		 = state.color_mask,
	};
}

void Renderer::SetRenderState(const RenderState& state) {
	if (state.viewport.has_value()) {
		SetViewport(*state.viewport);
	}
	if (state.view_projection.has_value()) {
		SetViewProjection(*state.view_projection);
	}
	if (state.blend_mode.has_value()) {
		SetBlendMode(*state.blend_mode);
	}
	if (state.depth_testing.has_value()) {
		SetDepthTesting(*state.depth_testing);
	}
	if (state.depth_mask.has_value()) {
		SetDepthMask(*state.depth_mask);
	}
	if (state.stencil.has_value()) {
		SetStencil(*state.stencil);
	}
	if (state.raster.has_value()) {
		SetRaster(*state.raster);
	}
	if (state.scissor.has_value()) {
		SetScissor(*state.scissor);
	}
	if (state.color_mask.has_value()) {
		SetColorMask(*state.color_mask);
	}
}

void Renderer::ApplyRenderState(const RenderState& state) {
	if (state.blend_mode.has_value()) {
		gl_->SetBlendMode(*state.blend_mode);
	}
	if (state.color_mask.has_value()) {
		gl_->SetColorMask(*state.color_mask);
	}
	if (state.depth_mask.has_value()) {
		gl_->SetDepthMask(*state.depth_mask);
	}
	if (state.depth_testing.has_value()) {
		gl_->SetDepthTesting(*state.depth_testing);
	}
	if (state.raster.has_value()) {
		gl_->SetRaster(*state.raster);
	}
	if (state.scissor.has_value()) {
		gl_->SetScissor(*state.scissor);
	}
	if (state.stencil.has_value()) {
		gl_->SetStencil(*state.stencil);
	}
	if (state.viewport.has_value()) {
		gl_->SetViewport(*state.viewport);
	}
	if (state.view_projection.has_value()) {
		view_projection_ = *state.view_projection;
		if (auto shader{ gl_->GetBoundShader() }; shader.has_value() && *shader) {
			gl_->shaders.SetUniform(*shader, "u_ViewProjection", view_projection_);
		}
	}
}

void Renderer::ApplyMaterial(const MaterialState& material) {
	auto _ = gl_->Bind(material.shader, false);

	SetUniform(material.shader, "u_ViewProjection", view_projection_);

	for (const UniformWrite& write : material.uniforms) {
		std::visit(
			[&]<typename T>(const T& value) {
				SetUniform(material.shader, write.name.c_str(), value);
			},
			write.value
		);
	}
}

void Renderer::DrawTexture(const DrawTextureRequest& request) {
	// TODO: Fix.
	// if (ReferencesBoundTarget(request)) {
	//	PTGN_ASSERT(
	//		IsFullscreenCompatible(request),
	//		"Sampling the bound target implies a fullscreen effect pass"
	//	);
	//	DrawFullscreenEffect(request);
	//	return;
	//}
	// if (request.effect_params.has_value()) {
	//	DrawTextureEffect(request);
	//	return;
	// }
	PTGN_ASSERT(
		request.extra_textures.empty(), "Cannot batch draw texture request with extra textures"
	);
	DrawTextureNormally(request);
}

void Renderer::DrawTextureNormally(const DrawTextureRequest& request) {
	std::span<const TextureId> textures;

	if (request.texture) {
		textures = { &request.texture, 1 };
	}

	if (request.transform.has_value()) {
		request.transform->ApplyTo(
			request.quads | std::views::join,
			[](const TextureVertex& vertex) {
				return V2_float{ vertex.position[0], vertex.position[1] };
			},
			[](TextureVertex& vertex, V2_float position) {
				vertex.position[0] = position.x;
				vertex.position[1] = position.y;
			}
		);
	}

	DrawQuads(
		DrawQuadRequest<TextureVertex, DefaultTextureIndexAccessor<TextureVertex>>{
			.quads{ request.quads }, .textures{ textures } }
	);
}

// TODO: Fix.
// void Renderer::DrawTextureEffect(DrawTextureRequest request) {
//	auto prepared = PrepareDirectTextureSeed(request);
//
//	auto result = RunEffectChain(
//		prepared.seed_texture, std::move(prepared.seed_transient), *request.effect_params
//	);
//
//	BatchEffectResult(std::move(result), std::span{ prepared.final_quads }, request.render_state);
//}

void Renderer::BindTextureSlot(std::uint32_t slot, TextureId texture) {
	gl_->SetActiveTextureSlot(slot);
	auto _3{ gl_->Bind(texture, false) };
}

} // namespace ptgn::impl