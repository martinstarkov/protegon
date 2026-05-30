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
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
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
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

void ApplyTransform(Transform transform, std::span<TextureQuad> local_quads) {
	transform.ApplyTo(
		local_quads | std::views::join,
		[](const TextureVertex& vertex) {
			return V2_float{ vertex.position[0], vertex.position[1] };
		},
		[](TextureVertex& vertex, V2_float position) {
			vertex.position[0] = position.x;
			vertex.position[1] = position.y;
		}
	);
}

/// @return True if all vertices in all quads have the same depth and entity ID, false otherwise.
bool HaveUniformDepthAndEntityId(const std::span<const impl::TextureQuad> quads) {
	PTGN_ASSERT(!quads.empty());

	const auto& first_quad{ quads.front() };
	const auto& first_vertex{ first_quad.front() };

	auto first_depth{ first_vertex.position[2] };
	auto first_entity_id{ first_vertex.entity_id[0] };

	for (auto& quad : quads) {
		for (auto& vertex : quad) {
			if (!NearlyEqual(vertex.position[2], first_depth) ||
				vertex.entity_id[0] != first_entity_id) {
				return false;
			}
		}
	}

	return true;
}

} // namespace impl

Renderer::Renderer(Window& window, Stats& stats, EventSink&& event_sink) :
	window_{ window },
	stats_{ stats },
	event_sink_{ std::move(event_sink) },
	gl_{ std::make_unique<impl::gl::GLContext>(stats) },
	batcher_{ *this },
	target_pool_{ *this },
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

	presentation_target_ =
		CreateRenderTarget({ .size{ display_size }, .format{ TextureFormat::RGBA8 } });
	BindPresentationTarget();
	SetViewProjection(display_size);

	auto max_texture_slots{ GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto quad{ GetShader("texture") };
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
	auto _{ gl_->Bind(impl::VertexArrayId{ 0 }, false) };
}

void Renderer::FlushBatch() {
	batcher_.Flush();
	temp_render_targets_.clear();
}

impl::RenderPipeline& Renderer::GetPipeline(impl::PipelineId id) {
	return pipeline_manager_.GetPipeline(id);
}

const impl::RenderPipeline& Renderer::GetPipeline(impl::PipelineId id) const {
	return pipeline_manager_.GetPipeline(id);
}

impl::RenderTargetObject Renderer::CreateRenderTarget(const RenderTargetDesc& desc) {
	PTGN_ASSERT(desc.size.IsPositive(), "Cannot create render target with zero size");

	impl::FramebufferId framebuffer{ 0 };

	if (IsColorFormat(desc.format)) {
		auto color{ gl_->textures.CreateTexture(desc.size, desc.format, desc.params) };
		framebuffer = gl_->framebuffers.Create(color);
	} else {
		auto depth{ gl_->renderbuffers.CreateRenderbuffer(desc.size, desc.format) };
		if (IsDepthOnlyFormat(desc.format)) {
			framebuffer = gl_->framebuffers.Create<impl::gl::Attachment::Depth>(depth);
		} else {
			framebuffer = gl_->framebuffers.Create<impl::gl::Attachment::DepthStencil>(depth);
		}
	}

	PTGN_ASSERT(framebuffer, "Failed to create valid framebuffer for render target");

	return impl::RenderTargetObject{ this, impl::RenderTargetId{ framebuffer } };
}

impl::TextureId Renderer::GetRenderTargetTexture(impl::RenderTargetId render_target) const {
	return gl_->framebuffers.GetAttachmentId(impl::FramebufferId{ render_target });
}

V2_int Renderer::GetRenderTargetSize(impl::RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };
	return GetTextureSize(id);
}

TextureFormat Renderer::GetRenderTargetTextureFormat(impl::RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };
	return GetTextureFormat(id);
}

TextureParams Renderer::GetRenderTargetTextureParams(impl::RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };
	return GetTextureParams(id);
}

void Renderer::ClearRenderTarget(
	impl::RenderTargetId render_target, Color color, bool set_viewport, bool restore_bind
) const {
	auto bind_guard = gl_->Bind(impl::FramebufferId{ render_target }, restore_bind);

	std::optional<Viewport> viewport;
	if (set_viewport) {
		viewport = gl_->GetViewport();

		auto render_target_size{ GetRenderTargetSize(render_target) };
		gl_->SetViewport({ .position{}, .size{ render_target_size } });
	}

	gl_->framebuffers.ClearColor(impl::FramebufferId{ render_target }, color);

	if (set_viewport && viewport.has_value() && viewport->size.IsPositive()) {
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

void Renderer::SetShader(std::string_view shader) {
	SetShader(GetShader(shader));
}

void Renderer::SetShader(impl::ShaderId shader) {
	const auto& bound{ gl_->GetBoundState() };
	if (shader == gl_->GetBoundState().shader_program) {
		return;
	}
	FlushBatch();
	auto _ = gl_->Bind(shader, false);
}

std::optional<BlendMode> Renderer::GetBlendMode() const {
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

void Renderer::SetRenderTarget(impl::RenderTargetObject* target) {
	impl::FramebufferId framebuffer{ target ? target->operator impl::RenderTargetId() : 0u };

	if (framebuffer == gl_->GetBoundFramebuffer()) {
		current_target_ = target;
		return;
	}

	FlushBatch();
	auto _ = gl_->Bind(framebuffer, false);

	current_target_ = target;
}

void Renderer::UpdateRenderTarget(impl::RenderTargetObject&& replacing_target) {
	PTGN_ASSERT(
		gl_->IsBound(impl::FramebufferId{ replacing_target.operator impl::RenderTargetId() }),
		"Render target that is replacing current render target must be bound"
	);
	PTGN_ASSERT(current_target_, "No current render target to update");
	*current_target_ = std::move(replacing_target);
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
	if (auto shader{ gl_->GetBoundShader() }; shader.has_value() && *shader) {
		gl_->shaders.SetUniform(*shader, "u_ViewProjection", *bound.view_projection);
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

bool Renderer::IsTextureAttachedToCurrentFramebuffer(impl::TextureId texture) const {
	auto bound{ gl_->GetBoundFramebuffer() };

	if (!bound.has_value() || *bound == impl::FramebufferId{ 0 }) {
		return false;
	}

	return gl_->framebuffers.GetAttachment(*bound) == texture;
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
		ResizePresentationTarget(display_viewport_.size);

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

void Renderer::ResizePresentationTarget(V2_int size) {
	ResizeRenderTarget(presentation_target_.resource_, size);
}

void Renderer::BindPresentationTarget() {
	SetRenderTarget(&presentation_target_);
}

impl::RenderTargetId Renderer::GetPresentationTarget() const {
	return presentation_target_.resource_;
}

void Renderer::InvalidateState() {
	gl_->InvalidateState();
}

void Renderer::BeginFrame() {
	InvalidateState();

	if (!presentation_viewport_.has_value()) {
		auto presentation{ GetPresentationViewport() };
		Color window_background_color{ window_.GetBackgroundColor() };

		auto _ = gl_->Bind(impl::FramebufferId{ 0 }, false);
		gl_->SetClearColor(window_background_color);
		SetViewport(presentation);
		gl_->framebuffers.Clear();
	}

	presentation_target_.Bind();
	presentation_target_.Clear(background_color_, false, false);
}

void Renderer::ApplyScreenEffects(const std::function<void(DrawContext&)>& screen_effect_callback) {
	if (!screen_effect_callback) {
		return;
	}

	FlushBatch();

	PTGN_ASSERT(presentation_target_, "Presentation target must be valid");

	auto size{ presentation_target_.GetSize() };

	PTGN_ASSERT(size.IsPositive(), "Presentation target size must be valid");

	SetRenderTarget(&presentation_target_);

	Viewport viewport{
		.position = {},
		.size	  = size,
	};

	SetViewport(viewport);
	SetViewProjection(viewport.size);
	SetScissor(ScissorState{ false });
	SetBlendMode(BlendMode::ReplaceRGBA);

	DrawContext ctx{ *this };

	screen_effect_callback(ctx);

	FlushBatch();
}

void Renderer::EndFrame(const std::function<void(DrawContext&)>& screen_effect_callback) {
	PTGN_ASSERT(display_viewport_.size.IsPositive());

	FlushBatch();

	ApplyScreenEffects(screen_effect_callback);

	SetRenderTarget(nullptr);

	if (presentation_viewport_.has_value()) {
		target_pool_.Update();
		PTGN_ASSERT(
			batcher_.IsEmpty(),
			"No indices should be left in the batcher after finishing the render frame"
		);
		return;
	}

	PTGN_ASSERT(
		presentation_target_.GetSize() == display_viewport_.size,
		"Screen target texture size must match display viewport size"
	);

	SetCurrentPipeline("texture");
	SetMaterial(
		MaterialState{
			.shader	  = GetShader("texture"),
			.uniforms = {},
		}
	);
	SetBlendMode(BlendMode::ReplaceRGBA);
	SetViewport(display_viewport_);
	SetViewProjection(display_viewport_.size);

	constexpr auto depth{ 0.0f };
	constexpr auto tint{ color::White };
	constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };
	constexpr auto entity_id{ -1 };

	auto local_vertices{ Rect{ display_viewport_.size }.GetLocalVertices() };
	auto local_quad{
		impl::CreateTextureQuad(local_vertices, depth, tint.Normalized(), tex_coords, entity_id)
	};

	impl::DrawTextureRequest request;
	request.local_quads = { &local_quad, 1 };
	request.texture		= presentation_target_.GetTextureId();

	DrawTexture(request);

	FlushBatch();

	target_pool_.Update();
	PTGN_ASSERT(
		batcher_.IsEmpty(),
		"No indices should be left in the batcher after finishing the render frame"
	);
}

bool Renderer::IsPresentationViewportVisible() const {
	return presentation_viewport_.has_value() && !presentation_viewport_->size.IsPositive();
}

impl::ShaderObject Renderer::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return impl::ShaderObject{ this, gl_->shaders.CreateProgram(source, shader_name) };
}

impl::TextureObject Renderer::CreateTexture(
	const std::uint8_t* pixel_data, V2_int size, TextureFormat format, TextureParams params
) {
	auto [pixel_format, pixel_type] = impl::gl::GetPixelDataFormat(format);
	PTGN_ASSERT(
		pixel_type == impl::gl::PixelDataType::UnsignedByte,
		"Texture format must have a type of bytes"
	);
	return impl::TextureObject{ this, gl_->textures.CreateTexture(
										  pixel_data, pixel_format, pixel_type, size, format, params
									  ) };
}

void Renderer::SetBoundShaderUniform(const char* uniform_name, int value) {
	auto shader{ gl_->GetBoundShader() };

	PTGN_ASSERT(
		shader.has_value() && *shader, "Shader must be bound before calling SetBoundShaderUniform"
	);

	gl_->shaders.SetUniform(*shader, uniform_name, value);
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

void Renderer::Destroy(impl::RenderTargetId id) {
	gl_->Destroy(id);
}

V2_int Renderer::GetTextureSize(impl::TextureId texture) const {
	return gl_->textures.GetTextureSize(texture);
}

TextureFormat Renderer::GetTextureFormat(impl::TextureId texture) const {
	return gl_->textures.GetTextureFormat(texture);
}

TextureParams Renderer::GetTextureParams(impl::TextureId texture) const {
	return gl_->textures.GetTextureParams(texture);
}

void Renderer::ResizeRenderTarget(impl::RenderTargetId render_target, V2_int new_size) {
	gl_->framebuffers.Resize(impl::FramebufferId{ render_target }, new_size);
}

void Renderer::SetTextureParams(impl::TextureId texture, TextureParams params) {
	using enum impl::gl::TextureParameter;
	gl_->textures.SetTextureParameter(texture, MinFilter, std::to_underlying(params.min_filter));
	gl_->textures.SetTextureParameter(texture, MagFilter, std::to_underlying(params.mag_filter));
	gl_->textures.SetTextureParameter(texture, WrapS, std::to_underlying(params.wrap_s));
	gl_->textures.SetTextureParameter(texture, WrapT, std::to_underlying(params.wrap_t));
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

const impl::RenderTargetObject& Renderer::GetBoundRenderTarget() const {
	PTGN_ASSERT(current_target_, "No current render target has been set");
	return *current_target_;
}

void Renderer::DrawRenderPass(impl::DrawPassRequest request) {
	FlushBatch();

	PTGN_ASSERT(request.output, "Render pass output must be valid");

	auto output_size{ GetRenderTargetSize(request.output) };

	PTGN_ASSERT(output_size.IsPositive(), "Render pass output size must be non-zero");
	PTGN_ASSERT(request.viewport.size.IsPositive(), "Render pass viewport size must be non-zero");

	auto previous_state{ GetRenderState() };
	auto previous_shader{ gl_->GetBoundShader() };
	auto previous_pipeline{ pipeline_manager_.GetCurrentPipelineId() };

	auto _ = gl_->Bind(impl::FramebufferId{ request.output }, false);

	SetCurrentPipeline(request.pipeline);
	SetMaterial(
		MaterialState{
			.shader	  = request.shader,
			.uniforms = {},
		}
	);

	// TODO: Somewhere in here the viewport is not being set correctly and right camera does not get
	// grayscale.

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
		auto texture{ GetRenderTargetTexture(input.render_target) };

		PTGN_ASSERT(texture, "Render pass input must have a valid color texture");

		auto input_size{ GetRenderTargetSize(input.render_target) };

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

	if (previous_shader.has_value()) {
		SetShader(*previous_shader);
	}

	SetRenderState(previous_state);
}

void Renderer::CopyRenderTargetRegion(
	impl::RenderTargetId source, impl::RenderTargetId destination, Viewport source_region,
	V2_int destination_position
) {
	// Batch must be flushed before copying framebuffer regions to ensure that all rendering
	// commands that may affect the source or destination regions are completed.
	FlushBatch();

	gl_->framebuffers.CopyRegion(
		impl::FramebufferId{ source }, impl::FramebufferId{ destination }, source_region,
		destination_position
	);
}

void Renderer::CompositeRenderPassResult(
	impl::RenderTargetId source, impl::RenderTargetId destination, Viewport destination_region
) {
	PTGN_ASSERT(source, "Render pass source must be valid");
	PTGN_ASSERT(destination, "Render pass destination must be valid");
	PTGN_ASSERT(destination_region.size.IsPositive(), "Composite region must be valid");

	auto source_size{ GetRenderTargetSize(source) };

	PTGN_ASSERT(
		source_size == V2_int{ destination_region.size },
		"Composite source size must match destination region size"
	);

	auto input{ impl::BoundInput{
		.render_target = source,
		.binding	   = TextureBinding{ 0, "u_Texture" },
	} };

	DrawRenderPass(
		impl::DrawPassRequest{
			.shader				 = GetShader("texture"),
			.pipeline			 = Hash("texture"),
			.inputs				 = std::span{ &input, 1 },
			.output				 = destination,
			.viewport			 = destination_region,
			.scissor_to_viewport = true,
		}
	);
}

void Renderer::SetRenderState(const RenderState& state) {
	if (state.viewport.has_value()) {
		SetViewport(*state.viewport);
	}
	if (state.view_projection.has_value()) {
		SetViewProjection(*state.view_projection);
	}
	if (state.blending.has_value()) {
		SetBlending(*state.blending);
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

void Renderer::DrawTexture(const impl::DrawTextureRequest& request) {
	if (request.local_quads.empty()) {
		return;
	}

	if (request.effect_params.draw_callback ||
		IsTextureAttachedToCurrentFramebuffer(request.texture)) {
		DrawTextureEffect(request);
	} else {
		DrawTextureNormally(request);
	}
}

void Renderer::DrawTextureNormally(const impl::DrawTextureRequest& request) {
	PTGN_ASSERT(!request.local_quads.empty());
	PTGN_ASSERT(!request.effect_params.draw_callback);

	std::span<const impl::TextureId> textures;

	if (request.texture) {
		textures = { &request.texture, 1 };
	}

	ApplyTransform(request.transform, request.local_quads);

	DrawQuads(request.local_quads, textures);
}

void Renderer::DrawTextureEffect(const impl::DrawTextureRequest& request) {
	PTGN_ASSERT(!request.local_quads.empty());

	PTGN_ASSERT(
		HaveUniformDepthAndEntityId(request.local_quads),
		"Batched effect vertices must have uniform depth and entity ID"
	);

	auto bounds{ Rect::FromPoints(
		request.local_quads | std::views::join |
		std::views::transform([](const impl::TextureVertex& vertex) {
			return V2_float{ vertex.position[0], vertex.position[1] };
		})
	) };

	PTGN_ASSERT(bounds.HasPositiveArea());

	auto format{ GetTextureFormat(request.texture) };
	auto params{ GetTextureParams(request.texture) };

	V2_float size{ bounds.GetSize() };

	PTGN_ASSERT(size.IsPositive());

	size += V2_float{ request.effect_params.margin * 2 };

	RenderTargetDesc desc{ .size = size, .format = format, .params = params };

	auto expanded_target{ CreateRenderTarget(desc) };

	PTGN_ASSERT(expanded_target.GetSize() == V2_int{ size });

	auto previous_target{ current_target_ };

	SetRenderTarget(&expanded_target);

	auto previous_pipeline{ pipeline_manager_.GetCurrentPipelineId() };
	auto previous_shader{ gl_->GetBoundShader() };
	auto previous_state{ GetRenderState() };

	Viewport viewport{ .position{}, .size{ size } };
	SetScissor(ScissorState{ viewport });
	SetViewport(viewport);
	SetViewProjection(size);

	impl::DrawTextureRequest local_request;

	local_request.local_quads = request.local_quads;
	local_request.texture	  = request.texture;

	DrawTextureNormally(local_request);

	FlushBatch();

	DrawContext ctx{ *this };

	if (request.effect_params.draw_callback) {
		request.effect_params.draw_callback(ctx);
	}

	SetCurrentPipeline(previous_pipeline);
	SetRenderTarget(previous_target);
	if (previous_shader.has_value()) {
		SetShader(*previous_shader);
	}
	SetRenderState(previous_state);

	impl::DrawTextureRequest new_request;

	auto positions{ Rect{ size }.GetLocalVertices() };

	PTGN_ASSERT(!request.local_quads.empty());

	const auto& first_quad{ request.local_quads.front() };

	PTGN_ASSERT(!first_quad.empty());

	const auto& first_vertex{ first_quad.front() };

	auto depth{ first_vertex.position[2] };

	constexpr auto color_n{ color::White.Normalized() };

	constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };

	auto entity_id{ first_vertex.entity_id[0] };

	auto local_quad{ impl::CreateTextureQuad(positions, depth, color_n, tex_coords, entity_id) };

	new_request.local_quads = { &local_quad, 1 };
	new_request.transform	= request.transform;
	new_request.texture		= GetRenderTargetTexture(expanded_target);

	DrawTextureNormally(new_request);

	temp_render_targets_.emplace_back(std::move(expanded_target));
}

void Renderer::BindTextureSlot(std::uint32_t slot, impl::TextureId texture) {
	gl_->SetActiveTextureSlot(slot);
	auto _3{ gl_->Bind(texture, false) };
}

namespace impl {

RendererAccessor::RendererAccessor(Renderer& renderer) : renderer_{ renderer } {}

TextureObject RendererAccessor::CreateTexture(
	const std::uint8_t* pixel_data, V2_int size, TextureFormat format, TextureParams params
) {
	return renderer_.CreateTexture(pixel_data, size, format, params);
}

ShaderObject RendererAccessor::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return renderer_.CreateShader(source, shader_name);
}

RenderTargetObject RendererAccessor::CreateRenderTarget(const RenderTargetDesc& desc) {
	return renderer_.CreateRenderTarget(desc);
}

TextureId RendererAccessor::GetPresentationTexture() const {
	return renderer_.GetRenderTargetTexture(renderer_.GetPresentationTarget());
}

void RendererAccessor::FlushBatch() {
	renderer_.FlushBatch();
}

void RendererAccessor::SetupPresentationTarget() {
	Viewport viewport{ {}, renderer_.GetDisplayViewport().size };

	renderer_.BindPresentationTarget();
	renderer_.SetViewport(viewport);
	renderer_.SetViewProjection(viewport.size);
	renderer_.SetBlendMode(BlendMode::Blend);
}

ShaderId RendererAccessor::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

void RendererAccessor::SetRenderTarget(RenderTargetObject* target) {
	renderer_.SetRenderTarget(target);
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

const RenderTargetObject& RendererAccessor::GetBoundRenderTarget() const {
	return renderer_.GetBoundRenderTarget();
}

} // namespace impl

} // namespace ptgn