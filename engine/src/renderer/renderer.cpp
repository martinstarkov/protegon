#include "renderer/renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
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
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/framebuffer_pool.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace {

constexpr TextureFormat kDefaultPresentationTargetFormat{ kDefaultHDRFormat };
constexpr const char* kViewProjectionUniform{ "u_ViewProjection" };

} // namespace

Renderer::Renderer(Window& window, Stats& stats, EventSink&& event_sink) :
	window_{ window },
	stats_{ stats },
	event_sink_{ std::move(event_sink) },
	gl_{ std::make_unique<impl::gl::GLContext>(stats) },
	batcher_{ *this },
	framebuffer_pool_{ *this },
	pipeline_manager_{ *this } {
	pipeline_manager_.AddPipeline<impl::TextureVertex>(
		"texture", impl::kVertexCapacity, impl::kIndexCapacity, impl::PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<impl::ShapeVertex>(
		"shape", impl::kVertexCapacity, impl::kIndexCapacity, impl::PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<impl::ColorVertex>(
		"color", impl::kVertexCapacity, impl::kIndexCapacity, impl::PrimitiveMode::Triangles
	);

	SetCurrentPipeline("texture");

	game_size_ = GetFullViewportSize();

	auto display{ RecalculateDisplayViewport() };

	display_viewport_		= display.viewport;
	display_viewport_dirty_ = false;

	auto display_size{ GetDisplaySize() };

	PTGN_ASSERT(display_size.IsPositive(), "Display size cannot be zero");

	presentation_framebuffer_ = CreateFramebuffer(
		{ .size{ display_size }, .format{ kDefaultPresentationTargetFormat } }, std::nullopt
	);
	BindPresentationFramebuffer();
	SetViewProjection(display_size);

	auto max_texture_slots{ GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::ranges::iota(samplers, 0);

	auto quad{ GetShader("texture") };
	auto _1 = gl_->Bind(quad, false);
	SetUniform(quad, impl::kTexturesUniform, samplers);

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
	auto _{ gl_->Bind(impl::VertexArrayId{ 0 }, false) };
}

void Renderer::FlushBatch() {
	batcher_.Flush();
	temp_framebuffers_.clear();
}

impl::RenderPipeline& Renderer::GetPipeline(impl::PipelineId id) {
	return pipeline_manager_.GetPipeline(id);
}

const impl::RenderPipeline& Renderer::GetPipeline(impl::PipelineId id) const {
	return pipeline_manager_.GetPipeline(id);
}

impl::FramebufferObject Renderer::CreateFramebuffer(
	TextureDesc desc, std::optional<TextureDesc> other_desc
) {
	PTGN_ASSERT(
		!other_desc.has_value() || desc != *other_desc,
		"Other texture description cannot match the first one"
	);

	PTGN_ASSERT(desc.size.IsPositive(), "Cannot create framebuffer with zero size");

	if (other_desc.has_value()) {
		PTGN_ASSERT(IsColorFormat(desc.format), "Cannot specify other_desc for non-color format");
		PTGN_ASSERT(
			other_desc->size == desc.size, "Framebuffer attachments must have matching sizes"
		);
		PTGN_ASSERT(
			!IsColorFormat(other_desc->format),
			"Other framebuffer attachment must be depth, stencil, or depth-stencil"
		);
	}

	impl::FramebufferId framebuffer{};

	if (IsColorFormat(desc.format)) {
		auto texture{ gl_->textures.Create(desc) };

		std::optional<impl::RenderbufferId> renderbuffer;
		auto attachment{ impl::gl::Attachment::DepthStencil };

		if (other_desc.has_value()) {
			attachment	 = impl::gl::GetDepthStencilAttachment(other_desc->format);
			renderbuffer = gl_->renderbuffers.Create(other_desc->size, other_desc->format);
		}

		framebuffer = gl_->framebuffers.Create(texture, renderbuffer, attachment, true);
	} else {
		PTGN_ASSERT(!other_desc.has_value(), "Cannot specify other_desc for non-color format");

		auto attachment{ impl::gl::GetDepthStencilAttachment(desc.format) };
		auto renderbuffer{ gl_->renderbuffers.Create(desc.size, desc.format) };

		framebuffer = gl_->framebuffers.Create(renderbuffer, attachment, true);
	}

	PTGN_ASSERT(framebuffer, "Failed to create valid framebuffer");

	return impl::FramebufferObject{ this, framebuffer };
}

impl::TextureId Renderer::GetTexture(impl::FramebufferId framebuffer) const {
	return gl_->framebuffers.GetAttachmentId(framebuffer);
}

impl::RenderbufferId Renderer::GetDepthRenderbuffer(impl::FramebufferId framebuffer) const {
	return gl_->framebuffers.GetAttachmentId<impl::gl::Attachment::Depth>(framebuffer);
}

impl::RenderbufferId Renderer::GetStencilRenderbuffer(impl::FramebufferId framebuffer) const {
	return gl_->framebuffers.GetAttachmentId<impl::gl::Attachment::Stencil>(framebuffer);
}

impl::RenderbufferId Renderer::GetDepthStencilRenderbuffer(impl::FramebufferId framebuffer) const {
	return gl_->framebuffers.GetAttachmentId<impl::gl::Attachment::DepthStencil>(framebuffer);
}

V2_int Renderer::GetSize(impl::FramebufferId framebuffer) const {
	auto texture{ GetTexture(framebuffer) };
	return GetSize(texture);
}

TextureFormat Renderer::GetFormat(impl::FramebufferId framebuffer) const {
	auto texture{ GetTexture(framebuffer) };
	return GetFormat(texture);
}

TextureParams Renderer::GetParams(impl::FramebufferId framebuffer) const {
	auto texture{ GetTexture(framebuffer) };
	return GetParams(texture);
}

TextureDesc Renderer::GetDesc(impl::FramebufferId framebuffer) const {
	auto texture{ GetTexture(framebuffer) };
	return GetDesc(texture);
}

void Renderer::Clear(impl::FramebufferId framebuffer, Color clear_color, bool restore_bind) const {
	auto bind_guard = gl_->Bind(framebuffer, restore_bind);
	gl_->framebuffers.Clear(framebuffer, clear_color);
}

void Renderer::Clear(impl::FramebufferId framebuffer, Depth clear_depth, bool restore_bind) const {
	auto bind_guard = gl_->Bind(framebuffer, restore_bind);
	gl_->framebuffers.Clear(framebuffer, clear_depth);
}

void Renderer::Clear(
	impl::FramebufferId framebuffer, Stencil clear_stencil, bool restore_bind
) const {
	auto bind_guard = gl_->Bind(framebuffer, restore_bind);
	gl_->framebuffers.Clear(framebuffer, clear_stencil);
}

void Renderer::Clear(
	impl::FramebufferId framebuffer, DepthStencil clear_depth_stencil, bool restore_bind
) const {
	auto bind_guard = gl_->Bind(framebuffer, restore_bind);
	gl_->framebuffers.Clear(framebuffer, clear_depth_stencil);
}

void Renderer::SetCurrentPipeline(std::string_view name) {
	SetCurrentPipeline(Hash(name));
}

void Renderer::SetCurrentPipeline(std::size_t id) {
	PTGN_ASSERT(id, "Cannot set current pipeline to 0");

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

void Renderer::SetShader(std::string_view shader) {
	SetShader(GetShader(shader));
}

void Renderer::SetShader(impl::ShaderId shader) {
	const auto& bound{ gl_->GetBoundState() };

	auto update_view_projection_uniform = [&]() {
		if (bound.render_state.view_projection.has_value()) {
			gl_->shaders.SetUniform(
				shader, kViewProjectionUniform, *bound.render_state.view_projection
			);
		}
	};

	if (shader == bound.shader_program) {
		update_view_projection_uniform();
		return;
	}
	FlushBatch();
	auto _ = gl_->Bind(shader, false);
	update_view_projection_uniform();
}

BlendMode Renderer::GetBlendMode() const {
	return gl_->GetBoundState().render_state.blend_mode;
}

void Renderer::SetBlendMode(BlendMode blend_mode, bool force) {
	if (!force && blend_mode == gl_->GetBoundState().render_state.blend_mode) {
		return;
	}
	FlushBatch();
	gl_->SetBlendMode(blend_mode);
}

void Renderer::SetBlending(bool enabled) {
	if (enabled == gl_->GetBoundState().render_state.blending) {
		return;
	}
	FlushBatch();
	gl_->SetBlend(enabled);
}

impl::PipelineId Renderer::GetTexturePipeline() const {
	return Hash("texture");
}

void Renderer::SetFramebuffer(impl::FramebufferObject* framebuffer) {
	impl::FramebufferId id{ framebuffer ? framebuffer->operator impl::FramebufferId() : 0u };

	if (id == gl_->GetBoundFramebuffer()) {
		current_framebuffer_ = framebuffer;
		return;
	}

	FlushBatch();
	auto _ = gl_->Bind(id, false);

	current_framebuffer_ = framebuffer;
}

void Renderer::SetViewProjection(V2_float size) {
	SetViewProjection(Matrix4::Orthographic(size));
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection != gl_->GetBoundState().render_state.view_projection) {
		FlushBatch();
		gl_->SetViewProjection(view_projection);
	}
	// TODO: Find a better way to do this. This is needed to ensure that the shader's
	// uniform is updated even if the shader itself doesn't change.
	const auto& bound{ gl_->GetBoundState().render_state };
	PTGN_ASSERT(bound.view_projection.has_value());
	if (auto shader{ GetBoundShader() }) {
		gl_->shaders.SetUniform(shader, kViewProjectionUniform, *bound.view_projection);
	}
}

void Renderer::SetDepthTesting(bool enabled) {
	if (enabled == gl_->GetBoundState().render_state.depth_testing) {
		return;
	}
	FlushBatch();
	gl_->SetDepthTesting(enabled);
}

void Renderer::SetDepthMask(const DepthMaskState& mask) {
	if (mask == gl_->GetBoundState().render_state.depth_mask) {
		return;
	}
	FlushBatch();
	gl_->SetDepthMask(mask);
}

void Renderer::SetStencil(const StencilState& stencil) {
	if (stencil == gl_->GetBoundState().render_state.stencil) {
		return;
	}
	FlushBatch();
	gl_->SetStencil(stencil);
}

void Renderer::SetRaster(const RasterState& raster) {
	if (raster == gl_->GetBoundState().render_state.raster) {
		return;
	}
	FlushBatch();
	gl_->SetRaster(raster);
}

void Renderer::SetScissor(const ScissorState& scissor) {
	if (scissor == gl_->GetBoundState().render_state.scissor) {
		return;
	}
	FlushBatch();
	gl_->SetScissor(scissor);
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	if (color_mask == gl_->GetBoundState().render_state.color_mask) {
		return;
	}
	FlushBatch();
	gl_->SetColorMask(color_mask);
}

impl::ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_->shaders.GetProgram(name);
}

bool Renderer::IsAttachedToCurrentFramebuffer(impl::TextureId texture) const {
	auto bound{ gl_->GetBoundFramebuffer() };

	if (!bound) {
		return false;
	}

	return gl_->framebuffers.GetAttachment(bound) == texture;
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
		!game_size.has_value() || game_size.has_value() && game_size->IsPositive(),
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

	PTGN_ASSERT(display_size.IsPositive());
	PTGN_ASSERT(game_size.IsPositive());

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
		ResizePresentationFramebuffer(display_viewport_.size);

		if (emit_events) {
			event_sink_(display_viewport_.size, ResizeType::Display);
		}
	}
}

Renderer::DisplayResizeInfo Renderer::RecalculateDisplayViewport() const {
	const auto presentation{ GetPresentationViewport() };

	PTGN_ASSERT(presentation.size.IsPositive());

	auto game_size{ game_size_.value_or(presentation.size) };

	PTGN_ASSERT(game_size.IsPositive());

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

	PTGN_ASSERT(viewport.size.IsPositive());

	return { .moved = moved, .resized = resized, .viewport{ viewport } };
}

void Renderer::ResizePresentationFramebuffer(V2_int size) {
	Resize(GetPresentationFramebuffer(), size);
}

void Renderer::BindPresentationFramebuffer() {
	SetFramebuffer(&presentation_framebuffer_);
}

impl::FramebufferId Renderer::GetPresentationFramebuffer() const {
	return presentation_framebuffer_.operator impl::FramebufferId();
}

void Renderer::ResetState() {
	gl_->ResetState();
}

void Renderer::BeginFrame() {
	ResetState();

	if (!presentation_viewport_.has_value()) {
		auto presentation{ GetPresentationViewport() };
		Color window_background_color{ window_.GetBackgroundColor() };

		auto _ = gl_->Bind(impl::FramebufferId{ 0 }, false);
		gl_->SetClearColor(window_background_color);
		SetViewport(presentation);
		gl_->framebuffers.Clear();
	}

	Clear(presentation_framebuffer_, background_color_, false);
}

void Renderer::BindUniforms() {
	auto shader{ GetBoundShader() };
	if (!shader) {
		return;
	}
	for (const auto& [name, value] : current_uniforms_) {
		SetUniformValue(shader, name.c_str(), value);
	}
}

impl::ShaderId Renderer::GetBoundShader() const {
	return gl_->GetBoundShader();
}

void Renderer::ExecuteEffectCallbacks(const std::function<void(DrawContext&)>& effect_callback) {
	DrawContext ctx{ *this };

	if (effect_callback) {
		effect_callback(ctx);
	}
}

void Renderer::DrawTexture(const impl::DrawTextureRequest& request) {
	Draw(request);
}

bool Renderer::IsPresentationViewportVisible() const {
	return presentation_viewport_.has_value() && !presentation_viewport_->size.IsPositive();
}

impl::ShaderObject Renderer::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return impl::ShaderObject{ this, gl_->shaders.CreateProgram(source, shader_name) };
}

impl::TextureObject Renderer::CreateTexture(const std::uint8_t* pixel_data, TextureDesc desc) {
	auto [pixel_format, pixel_type] = impl::gl::GetPixelDataFormat(desc.format);
	PTGN_ASSERT(
		pixel_type == impl::gl::PixelDataType::UnsignedByte,
		"Texture format must have a type of bytes"
	);
	return impl::TextureObject{ this,
								gl_->textures.Create(pixel_data, pixel_format, pixel_type, desc) };
}

void Renderer::SetBoundShaderUniform(const char* uniform_name, int value) {
	auto shader{ GetBoundShader() };

	PTGN_ASSERT(shader, "Shader must be bound before calling SetBoundShaderUniform");

	gl_->shaders.SetUniform(shader, uniform_name, value);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, const Matrix4& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V2_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V3_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V4_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(
	impl::ShaderId shader, const char* uniform_name, std::span<const float> v
) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V2_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V3_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V4_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, std::span<const int> v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, bool v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniformValue(
	impl::ShaderId shader, const char* uniform_name, const UniformValue& v
) {
	std::visit([&]<typename T>(T&& s) { SetUniform(shader, uniform_name, std::forward<T>(s)); }, v);
}

void Renderer::Destroy(impl::VertexBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::ElementBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::UniformBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::ShaderId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::TextureId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::RenderbufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::FramebufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::VertexArrayId id) {
	gl_->Destroy(id);
}

V2_int Renderer::GetSize(impl::TextureId texture) const {
	return GetDesc(texture).size;
}

TextureFormat Renderer::GetFormat(impl::TextureId texture) const {
	return GetDesc(texture).format;
}

TextureParams Renderer::GetParams(impl::TextureId texture) const {
	return GetDesc(texture).params;
}

TextureDesc Renderer::GetDesc(impl::TextureId texture) const {
	return gl_->textures.GetDesc(texture);
}

V2_int Renderer::GetSize(impl::RenderbufferId renderbuffer) const {
	return gl_->renderbuffers.GetSize(renderbuffer);
}

TextureFormat Renderer::GetFormat(impl::RenderbufferId renderbuffer) const {
	return gl_->renderbuffers.GetFormat(renderbuffer);
}

void Renderer::Resize(impl::FramebufferId framebuffer, V2_int new_size) {
	gl_->framebuffers.Resize(framebuffer, new_size);
}

void Renderer::SetParams(impl::FramebufferId framebuffer, TextureParams params) {
	auto texture{ GetTexture(framebuffer) };
	SetParams(texture, params);
}

void Renderer::SetParams(impl::TextureId texture, TextureParams params) {
	using enum impl::gl::TextureParameter;
	gl_->textures.SetParameter(texture, MinFilter, std::to_underlying(params.min_filter));
	gl_->textures.SetParameter(texture, MagFilter, std::to_underlying(params.mag_filter));
	gl_->textures.SetParameter(texture, WrapS, std::to_underlying(params.wrap_s));
	gl_->textures.SetParameter(texture, WrapT, std::to_underlying(params.wrap_t));
}

std::size_t Renderer::GetMaxTextureSlots() const {
	return gl_->GetMaxTextureSlots();
}

void Renderer::UploadVertices(
	const impl::RenderPipeline& pipeline, std::span<const std::byte> vertices,
	std::uint32_t vertex_size
) {
	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _{ gl_->Bind(pipeline.vbo, false) };

	auto vertex_count{ static_cast<std::uint32_t>(vertices.size() / vertex_size) };

	gl_->buffers.SetBufferSubData<impl::VertexBufferId>(
		pipeline.vbo, impl::gl::BufferTarget::ArrayBuffer, vertices.data(), 0, vertex_count,
		vertex_size
	);
}

void Renderer::UploadIndices(
	const impl::RenderPipeline& pipeline, std::span<const impl::Index> indices
) {
	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _{ gl_->Bind(pipeline.ebo, false) };

	gl_->buffers.SetBufferSubData<impl::ElementBufferId>(
		pipeline.ebo, impl::gl::BufferTarget::ElementArrayBuffer, indices.data(), 0,
		static_cast<std::uint32_t>(indices.size()), sizeof(impl::Index)
	);
}

void Renderer::DrawElements(const impl::RenderPipeline& pipeline, std::uint32_t index_count) {
	auto _0{ gl_->Bind(pipeline.vao, false) };

	gl_->vertex_arrays.DrawElements(
		pipeline.vao, index_count, impl::gl::IndexType::UnsignedInt, pipeline.primitive_mode
	);
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
	return state.render_state;
}

const impl::FramebufferObject& Renderer::GetBoundFramebuffer() const {
	PTGN_ASSERT(current_framebuffer_, "No current framebuffer has been set");
	return *current_framebuffer_;
}

impl::FramebufferObject& Renderer::GetBoundFramebuffer() {
	PTGN_ASSERT(current_framebuffer_, "No current framebuffer has been set");
	return *current_framebuffer_;
}

void Renderer::DrawRenderPass(const impl::DrawPassRequest& request) {
	FlushBatch();

	PTGN_ASSERT(request.output, "Render pass output must be valid");

	auto output_size{ GetSize(request.output) };

	PTGN_ASSERT(output_size.IsPositive(), "Render pass output size must be non-zero");
	PTGN_ASSERT(request.viewport.size.IsPositive(), "Render pass viewport size must be non-zero");

	auto previous_state{ GetRenderState() };
	auto previous_shader{ GetBoundShader() };
	auto previous_pipeline{ pipeline_manager_.GetCurrentPipelineId() };

	auto _ = gl_->Bind(request.output, false);

	SetCurrentPipeline(request.pipeline);
	SetMaterial(request.material);

	if (request.scissor_to_viewport) {
		SetScissor(ScissorState{ request.viewport });
	} else {
		SetScissor(ScissorState{ false });
	}

	SetViewport(request.viewport);
	SetViewProjection(request.viewport.size);
	SetBlendMode(BlendMode::ReplaceRGBA);

	std::vector<TextureBinding> bindings;
	std::vector<impl::TextureId> textures;

	bindings.reserve(request.inputs.size());
	textures.reserve(request.inputs.size());

	for (const auto& input : request.inputs) {
		auto texture{ GetTexture(input.framebuffer) };

		PTGN_ASSERT(texture, "Render pass input must have a valid color texture");

		auto input_size{ GetSize(input.framebuffer) };

		PTGN_ASSERT(input_size.IsPositive(), "Render pass input size must be non-zero");

		PTGN_ASSERT(
			input_size == request.viewport.size,
			"Render pass input size must match the draw viewport size"
		);

		bindings.emplace_back(input.binding);
		textures.emplace_back(texture);
	}

	constexpr auto depth{ 0.0f };
	constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };
	constexpr auto entity_id{ -1 };

	auto local_vertices{ Rect{ request.viewport.size }.GetLocalVertices() };

	auto local_quad{ impl::CreateTextureQuad(
		local_vertices, depth, request.tint.Normalized(), tex_coords, entity_id
	) };

	std::span quads{ &local_quad, 1 };

	const auto& pipeline{ pipeline_manager_.GetCurrentPipeline() };

	batcher_.SubmitQuadsWithTextureBindings<impl::TextureVertex>(
		quads, pipeline.vertex_capacity, pipeline.index_capacity, bindings, textures
	);

	FlushBatch();

	SetCurrentPipeline(previous_pipeline);
	SetShader(previous_shader);
	SetRenderState(previous_state);
}

void Renderer::CopyFramebufferRegion(
	impl::FramebufferId source, impl::FramebufferId destination, Viewport source_region,
	V2_int destination_position
) {
	// Batch must be flushed before copying framebuffer regions to ensure that all rendering
	// commands that may affect the source or destination regions are completed.
	FlushBatch();

	gl_->framebuffers.CopyRegion(source, destination, source_region, destination_position);
}

void Renderer::CompositeRenderPassResult(
	impl::FramebufferId source, impl::FramebufferId destination, Viewport destination_region
) {
	PTGN_ASSERT(source, "Render pass source must be valid");
	PTGN_ASSERT(destination, "Render pass destination must be valid");
	PTGN_ASSERT(destination_region.size.IsPositive(), "Composite region must be valid");

	auto source_size{ GetSize(source) };

	PTGN_ASSERT(
		source_size == V2_int{ destination_region.size },
		"Composite source size must match destination region size"
	);

	auto input{ impl::BoundInput{
		.framebuffer = source,
		.binding	 = TextureBinding{ 0, kTextureUniform },
	} };

	DrawRenderPass(
		impl::DrawPassRequest{ .material			= { .shader = GetShader("passthrough") },
							   .pipeline			= Hash("texture"),
							   .inputs				= std::span{ &input, 1 },
							   .output				= destination,
							   .viewport			= destination_region,
							   .scissor_to_viewport = true }
	);
}

void Renderer::SetRenderState(const RenderState& state) {
	SetViewport(state.viewport);
	if (state.view_projection.has_value()) {
		SetViewProjection(*state.view_projection);
	}
	SetBlending(state.blending);
	SetBlendMode(state.blend_mode);
	SetDepthTesting(state.depth_testing);
	SetDepthMask(state.depth_mask);
	SetStencil(state.stencil);
	SetRaster(state.raster);
	SetScissor(state.scissor);
	SetColorMask(state.color_mask);
}

void Renderer::SetRenderStateDelta(const RenderStateDelta& delta) {
	if (delta.viewport.has_value()) {
		SetViewport(*delta.viewport);
	}
	if (delta.view_projection.has_value()) {
		SetViewProjection(*delta.view_projection);
	}
	if (delta.blending.has_value()) {
		SetBlending(*delta.blending);
	}
	if (delta.blend_mode.has_value()) {
		SetBlendMode(*delta.blend_mode);
	}
	if (delta.depth_testing.has_value()) {
		SetDepthTesting(*delta.depth_testing);
	}
	if (delta.depth_mask.has_value()) {
		SetDepthMask(*delta.depth_mask);
	}
	if (delta.stencil.has_value()) {
		SetStencil(*delta.stencil);
	}
	if (delta.raster.has_value()) {
		SetRaster(*delta.raster);
	}
	if (delta.scissor.has_value()) {
		SetScissor(*delta.scissor);
	}
	if (delta.color_mask.has_value()) {
		SetColorMask(*delta.color_mask);
	}
}

void Renderer::BindTextureSlot(std::uint32_t slot, impl::TextureId texture) {
	gl_->SetActiveTextureSlot(slot);
	auto _3{ gl_->Bind(texture, false) };
}

bool Renderer::FramebufferMatches(
	impl::FramebufferId framebuffer, TextureDesc desc, std::optional<TextureDesc> other_desc
) const {
	using enum impl::gl::Attachment;
	using enum impl::gl::AttachmentStorage;

	if (IsColorFormat(desc.format)) {
		if (auto expected_depth_stencil{
				other_desc.has_value()
					? std::optional{ impl::gl::GetDepthStencilAttachment(other_desc->format) }
					: std::nullopt };
			!gl_->framebuffers.HasOnlyAttachmentLayout(
				framebuffer, Color0, expected_depth_stencil
			)) {
			return false;
		}

		auto color{ gl_->framebuffers.FindAttachment(framebuffer, Color0, Texture) };

		PTGN_ASSERT(color.has_value());

		auto color_texture{ impl::TextureId{ color->id } };

		if (GetFormat(color_texture) != desc.format || GetSize(color_texture) != desc.size) {
			return false;
		}

		if (!other_desc.has_value()) {
			return true;
		}

		auto depth_stencil_attachment{ impl::gl::GetDepthStencilAttachment(other_desc->format) };
		auto depth_stencil{
			gl_->framebuffers.FindAttachment(framebuffer, depth_stencil_attachment, Renderbuffer)
		};

		PTGN_ASSERT(depth_stencil.has_value());

		auto renderbuffer{ impl::RenderbufferId{ depth_stencil->id } };

		return GetFormat(renderbuffer) == other_desc->format &&
			   GetSize(renderbuffer) == other_desc->size;
	}

	auto expected_attachment{ impl::gl::GetDepthStencilAttachment(desc.format) };

	if (!gl_->framebuffers.HasOnlyAttachmentLayout(
			framebuffer, std::nullopt, expected_attachment
		)) {
		return false;
	}

	auto depth_stencil{
		gl_->framebuffers.FindAttachment(framebuffer, expected_attachment, Renderbuffer)
	};

	PTGN_ASSERT(depth_stencil.has_value());

	auto renderbuffer{ impl::RenderbufferId{ depth_stencil->id } };

	return GetFormat(renderbuffer) == desc.format && GetSize(renderbuffer) == desc.size;
}

namespace impl {

RendererAccessor::RendererAccessor(Renderer& renderer) : renderer_{ renderer } {}

TextureObject RendererAccessor::CreateTexture(const std::uint8_t* pixel_data, TextureDesc desc) {
	return renderer_.CreateTexture(pixel_data, desc);
}

ShaderObject RendererAccessor::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return renderer_.CreateShader(source, shader_name);
}

FramebufferObject RendererAccessor::CreateFramebuffer(
	TextureDesc desc, std::optional<TextureDesc> other_desc
) {
	return renderer_.CreateFramebuffer(desc, other_desc);
}

TextureId RendererAccessor::GetPresentationTexture() const {
	return GetTexture(renderer_.GetPresentationFramebuffer());
}

TextureId RendererAccessor::GetTexture(FramebufferId framebuffer) const {
	return renderer_.GetTexture(framebuffer);
}

void RendererAccessor::FlushBatch() {
	renderer_.FlushBatch();
}

void RendererAccessor::SetupPresentationFramebuffer() {
	Viewport viewport{ {}, renderer_.GetDisplayViewport().size };

	renderer_.BindPresentationFramebuffer();
	renderer_.SetViewport(viewport);
	renderer_.SetViewProjection(viewport.size);
	renderer_.SetBlendMode(BlendMode::Blend);
}

ShaderId RendererAccessor::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

void RendererAccessor::SetFramebuffer(FramebufferObject* framebuffer) {
	renderer_.SetFramebuffer(framebuffer);
}

void RendererAccessor::SetScissor(const ScissorState& scissor) {
	renderer_.SetScissor(scissor);
}

void RendererAccessor::SetViewProjection(const Matrix4& view_projection) {
	renderer_.SetViewProjection(view_projection);
}

void RendererAccessor::SetViewport(Viewport viewport) {
	renderer_.SetViewport(viewport);
}

void RendererAccessor::SetBlendMode(BlendMode blend_mode, bool force) {
	renderer_.SetBlendMode(blend_mode, force);
}

const FramebufferObject& RendererAccessor::GetBoundFramebuffer() const {
	return renderer_.GetBoundFramebuffer();
}

} // namespace impl

} // namespace ptgn