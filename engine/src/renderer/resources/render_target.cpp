#include "renderer/resources/render_target.h"

#include <optional>
#include <utility>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl {

V2_int RenderTarget::GetSize() const {
	return size_;
}

TextureFormat RenderTarget::GetFormat() const {
	return format_;
}

RenderTarget::RenderTarget(
	gl::GLContext* gl, const Framebuffer& framebuffer, const std::optional<Texture>& color,
	const std::optional<Renderbuffer>& depth, V2_int size, TextureFormat format
) :
	gl_{ gl },
	framebuffer_{ framebuffer },
	color_{ color },
	depth_{ depth },
	size_{ size },
	format_{ format } {}

RenderTarget::RenderTarget(gl::GLContext* gl, V2_int size, TextureFormat format) :
	gl_{ gl }, size_{ size }, format_{ format } {
	PTGN_ASSERT(gl_ != nullptr);

	const auto& desc = gl::GetTextureFormatDesc(format_);

	color_ = gl_->textures.CreateTexture(
		nullptr, desc.pixel_format, desc.pixel_type, size_, desc.internal_format
	);

	if (desc.has_depth || desc.has_stencil) {
		auto rb_format{ desc.has_stencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT };

		depth_ = gl_->renderbuffers.CreateRenderbuffer(size_, rb_format);
	}

	framebuffer_ = gl_->framebuffers.CreateFramebuffer(
		color_, gl::Attachment::Color0, depth_,
		desc.has_stencil ? gl::Attachment::DepthStencil : gl::Attachment::Depth
	);
}

void RenderTarget::Resize(V2_int new_size) {
	if (size_ == new_size) {
		return;
	}

	PTGN_ASSERT(gl_ != nullptr);
	gl_->framebuffers.ResizeFramebuffer(framebuffer_, new_size);

	size_ = new_size;
}

void RenderTarget::Destroy() {
	if (!gl_) {
		return;
	}

	if (color_.has_value()) {
		gl_->textures.DestroyTexture(*color_);
	}
	if (depth_.has_value()) {
		gl_->renderbuffers.DestroyRenderbuffer(*depth_);
	}

	gl_->framebuffers.DestroyFramebuffer(framebuffer_);

	size_	= {};
	format_ = {};
}

void RenderTarget::Clear(Color color) {
	PTGN_ASSERT(gl_ != nullptr);

	auto bind_guard = gl_->Bind(framebuffer_, true);

	gl_->SetViewport({ {}, size_ });

	gl_->framebuffers.ClearToColor(framebuffer_, color);
}

void RenderTarget::Bind() {
	PTGN_ASSERT(gl_ != nullptr);

	auto _ = gl_->Bind(framebuffer_);

	gl_->SetViewport({ {}, size_ });
}

RenderTarget::~RenderTarget() noexcept {
	Destroy();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept :
	gl_{ std::exchange(other.gl_, nullptr) },
	framebuffer_{ std::exchange(other.framebuffer_, Framebuffer{ 0 }) },
	color_{ std::exchange(other.color_, {}) },
	depth_{ std::exchange(other.depth_, {}) },
	size_{ std::exchange(other.size_, {}) },
	format_{ std::exchange(other.format_, TextureFormat::RGBA8) } {}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
	if (this != &other) {
		Destroy();

		color_		 = std::exchange(other.color_, {});
		depth_		 = std::exchange(other.depth_, {});
		format_		 = std::exchange(other.format_, TextureFormat::RGBA8);
		framebuffer_ = std::exchange(other.framebuffer_, Framebuffer{ 0 });
		gl_			 = std::exchange(other.gl_, nullptr);
		size_		 = std::exchange(other.size_, {});
	}

	return *this;
}

} // namespace ptgn::impl

// void GameResizeScript::OnGameSizeChanged() {
//	auto game_size{ Application::Get().render_.GetGameSize() };
//	RenderTarget{ entity }.Resize(game_size);
// }
//
// void DisplayResizeScript::OnDisplaySizeChanged() {
//	auto display_size{ Application::Get().render_.GetDisplaySize() };
//	RenderTarget{ entity }.Resize(display_size);
// }
//
// } // namespace impl
//
// RenderTarget::RenderTarget(const Entity& entity) : Entity{ entity } {}
//
// void RenderTarget::Draw(const Entity& entity) {
//	Sprite sprite{ entity };
//
//	Camera camera{ RenderTarget{ entity }.GetCamera() };
//
//	V2_float texture_size;
//
//	if (camera) {
//		texture_size = camera.GetViewportSize();
//	}
//
//	if (texture_size.IsZero()) {
//		texture_size = sprite.GetSize();
//	}
//
//	Application::Get().render_.DrawTexture(
//		sprite.GetTexture(), GetDrawTransform(entity), texture_size, GetDrawOrigin(entity),
//		GetTint(entity), GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
//		entity.GetOrDefault<PreFX>(), entity.GetOrDefault<PostFX>(),
//		sprite.GetTextureCoordinates(true)
//	);
// }
//
// V2_int RenderTarget::GetTextureSize() const {
//	return impl::GetTextureSize(*this);
// }
//
// V2_int RenderTarget::GetSize() const {
//	return impl::GetCroppedSize(*this);
// }
//
// V2_float RenderTarget::GetDisplaySize() const {
//	return impl::GetDisplaySize(*this);
// }
//
// const Camera& RenderTarget::GetCamera() const {
//	PTGN_ASSERT(Has<GameObject<Camera>>());
//	return Get<GameObject<Camera>>();
// }
//
// Camera& RenderTarget::GetCamera() {
//	PTGN_ASSERT(Has<GameObject<Camera>>());
//	return Get<GameObject<Camera>>();
// }
//
// void RenderTarget::Bind() const {
//	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
//	PTGN_ASSERT(frame_buffer.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
//	frame_buffer.Bind();
//	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind render target frame buffer");
// }
//
// void RenderTarget::Clear() const {
//	auto clear_color{ GetOrDefault<impl::ClearColor>() };
//	ClearToColor(clear_color);
// }
//
// void RenderTarget::ClearToColor(const Color& color) const {
//	PTGN_ASSERT(Has<impl::FrameBuffer>(), "Cannot clear render target with no frame buffer");
//	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
//	frame_buffer.Bind();
//	PTGN_ASSERT(IsBound(), "Frame buffer must be bound before clearing");
//	impl::GLRenderer::ClearToColor(color);
// }
//
// void RenderTarget::ClearDisplayList() {
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	auto& display_list{ Get<impl::DisplayList>().entities };
//	for (Entity entity : display_list) {
//		if (entity) {
//			entity.Remove<RenderTarget>();
//		}
//	}
//	display_list.clear();
// }
//
// void RenderTarget::AddToDisplayList(Entity entity) {
//	PTGN_ASSERT(entity, "Cannot add invalid entity to render target");
//	PTGN_ASSERT(HasDraw(entity), "Entity added to render target display list must be drawable");
//	// TODO: Consider allowing render targets to be rendered to other render targets.
//	PTGN_ASSERT(
//		!entity.Has<impl::FrameBuffer>(),
//		"Cannot add a render target to the display list of another render target. This is because "
//		"render order of targets is not enforced. Perhaps in the future."
//	);
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	auto& dl{ Get<impl::DisplayList>().entities };
//	dl.emplace_back(entity);
//	entity.Add<RenderTarget>(*this);
// }
//
// void RenderTarget::RemoveFromDisplayList(Entity entity) {
//	PTGN_ASSERT(entity, "Cannot remove invalid entity from render target");
//	PTGN_ASSERT(HasDraw(entity), "Entity remove from render target display list must be drawable");
//	entity.Remove<RenderTarget>();
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	auto& dl{ Get<impl::DisplayList>().entities };
//	std::erase(dl, entity);
// }
//
// const std::vector<Entity>& RenderTarget::GetDisplayList() const {
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	return Get<impl::DisplayList>().entities;
// }
//
// std::vector<Entity>& RenderTarget::GetDisplayList() {
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	return Get<impl::DisplayList>().entities;
// }
//
// Color RenderTarget::GetClearColor() const {
//	return GetOrDefault<impl::ClearColor>();
// }
//
// void RenderTarget::SetClearColor(const Color& clear_color) {
//	Add<impl::ClearColor>(clear_color);
// }
//
// const impl::Texture& RenderTarget::GetTexture() const {
//	return GetFrameBuffer().GetTexture();
// }
//
// const impl::FrameBuffer& RenderTarget::GetFrameBuffer() const {
//	return Get<impl::FrameBuffer>();
// }
//
// Color RenderTarget::GetPixel(const V2_int& coordinate, bool restore_bind_state) const {
//	return GetFrameBuffer().GetPixel(coordinate, restore_bind_state);
// }
//
// void RenderTarget::ForEachPixel(
//	const std::function<void(V2_int, Color)>& func, bool restore_bind_state
//) const {
//	return GetFrameBuffer().ForEachPixel(func, restore_bind_state);
// }
//
// void RenderTarget::Resize(const V2_int& size) {
//	if (auto camera{ TryGet<GameObject<Camera>>() }; camera && !camera->IsGameCamera()) {
//		Camera::Resize(*camera, size, true, true);
//	}
//	Get<impl::FrameBuffer>().Resize(size);
// }
//
// RenderTarget& SetDrawFilterImpl(RenderTarget& render_target, std::string_view filter_name) {
//	EntityAccess::Add<IDrawFilter>(render_target, filter_name);
//	return render_target;
// }
//
// bool RenderTarget::HasDrawFilter() const {
//	return Has<impl::IDrawFilter>();
// }
//
// RenderTarget& RenderTarget::RemoveDrawFilter() {
//	impl::EntityAccess::Remove<impl::IDrawFilter>(*this);
//	return *this;
// }