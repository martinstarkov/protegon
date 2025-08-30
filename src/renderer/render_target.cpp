#include "renderer/render_target.h"

#include <functional>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/drawable.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "scene/camera.h"

namespace ptgn {

namespace impl {

RenderTarget AddRenderTargetComponents(
	const Entity& entity, Manager& manager, const V2_int& size, const Color& clear_color,
	TextureFormat texture_format
) {
	PTGN_ASSERT(entity);

	RenderTarget render_target{ entity };

	SetPosition(render_target, {});

	render_target.Add<TextureHandle>();
	render_target.Add<impl::DisplayList>();
	render_target.Add<impl::ClearColor>(clear_color);
	auto& camera{ render_target.Add<GameObject<Camera>>(CreateCamera(manager)) };
	camera.SetViewport(-size * 0.5f, size);
	SetDraw<RenderTarget>(render_target);
	Show(render_target);

	// TODO: Move frame buffer object to a FrameBufferManager.
	const auto& frame_buffer{ render_target.Add<impl::FrameBuffer>(impl::Texture{
		nullptr, size, texture_format }) };

	PTGN_ASSERT(frame_buffer.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind frame buffer for render target");

	render_target.Clear();

	return render_target;
}

void LogicalRenderTargetResizeScript::OnLogicalResolutionChanged() {
	auto logical_resolution{ game.renderer.GetLogicalResolution() };
	RenderTarget{ entity }.Resize(logical_resolution);
}

void PhysicalRenderTargetResizeScript::OnPhysicalResolutionChanged() {
	auto physical_resolution{ game.renderer.GetPhysicalResolution() };
	RenderTarget{ entity }.Resize(physical_resolution);
}

} // namespace impl

RenderTarget::RenderTarget(const Entity& entity) : Entity{ entity } {}

void RenderTarget::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawTexture(ctx, entity, true);
}

V2_int RenderTarget::GetTextureSize() const {
	return impl::GetTextureSize(*this);
}

V2_int RenderTarget::GetSize() const {
	return impl::GetCroppedSize(*this);
}

V2_float RenderTarget::GetDisplaySize() const {
	return impl::GetDisplaySize(*this);
}

void RenderTarget::Bind() const {
	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	PTGN_ASSERT(frame_buffer.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer.Bind();
	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind render target frame buffer");
}

void RenderTarget::Clear() const {
	auto clear_color{ GetOrDefault<impl::ClearColor>() };
	ClearToColor(clear_color);
}

void RenderTarget::ClearToColor(const Color& color) const {
	PTGN_ASSERT(Has<impl::FrameBuffer>(), "Cannot clear render target with no frame buffer");
	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	frame_buffer.ClearToColor(color);
}

void RenderTarget::ClearDisplayList() {
	PTGN_ASSERT(Has<impl::DisplayList>());
	auto& display_list{ Get<impl::DisplayList>().entities };
	for (Entity entity : display_list) {
		if (entity) {
			entity.Remove<RenderTarget>();
		}
	}
	display_list.clear();
}

void RenderTarget::AddToDisplayList(Entity& entity) {
	PTGN_ASSERT(entity, "Cannot add invalid entity to render target");
	PTGN_ASSERT(HasDraw(entity), "Entity added to render target display list must be drawable");
	// TODO: Consider allowing render targets to be rendered to other render targets.
	PTGN_ASSERT(
		!entity.Has<impl::FrameBuffer>(),
		"Cannot add a render target to the display list of another render target. This is because "
		"render order of targets is not enforced. Perhaps in the future."
	);
	PTGN_ASSERT(Has<impl::DisplayList>());
	auto& dl{ Get<impl::DisplayList>().entities };
	dl.emplace_back(entity);
	entity.Add<RenderTarget>(*this);
}

void RenderTarget::RemoveFromDisplayList(Entity& entity) {
	PTGN_ASSERT(entity, "Cannot remove invalid entity from render target");
	PTGN_ASSERT(HasDraw(entity), "Entity remove from render target display list must be drawable");
	entity.Remove<RenderTarget>();
	PTGN_ASSERT(Has<impl::DisplayList>());
	auto& dl{ Get<impl::DisplayList>().entities };
	std::erase(dl, entity);
}

const std::vector<Entity>& RenderTarget::GetDisplayList() const {
	PTGN_ASSERT(Has<impl::DisplayList>());
	return Get<impl::DisplayList>().entities;
}

std::vector<Entity>& RenderTarget::GetDisplayList() {
	PTGN_ASSERT(Has<impl::DisplayList>());
	return Get<impl::DisplayList>().entities;
}

Color RenderTarget::GetClearColor() const {
	return GetOrDefault<impl::ClearColor>();
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	Add<impl::ClearColor>(clear_color);
}

const impl::Texture& RenderTarget::GetTexture() const {
	return GetFrameBuffer().GetTexture();
}

const impl::FrameBuffer& RenderTarget::GetFrameBuffer() const {
	return Get<impl::FrameBuffer>();
}

Color RenderTarget::GetPixel(const V2_int& coordinate, bool restore_bind_state) const {
	return GetFrameBuffer().GetPixel(coordinate, restore_bind_state);
}

void RenderTarget::ForEachPixel(
	const std::function<void(V2_int, Color)>& func, bool restore_bind_state
) const {
	return GetFrameBuffer().ForEachPixel(func, restore_bind_state);
}

void RenderTarget::Resize(const V2_int& size) {
	Get<impl::FrameBuffer>().GetTexture().Resize(size);
}

RenderTarget CreateRenderTarget(
	Manager& manager, ResizeToResolution resize_to_resolution, const Color& clear_color,
	TextureFormat texture_format
) {
	RenderTarget render_target{ manager.CreateEntity() };

	V2_int resolution;

	if (resize_to_resolution == ResizeToResolution::Physical) {
		resolution = game.renderer.GetPhysicalResolution();
		AddScript<impl::PhysicalRenderTargetResizeScript>(render_target);
	} else if (resize_to_resolution == ResizeToResolution::Logical) {
		resolution = game.renderer.GetLogicalResolution();
		AddScript<impl::LogicalRenderTargetResizeScript>(render_target);
	} else {
		PTGN_ERROR("Unknown resize to resolution value");
	}

	PTGN_ASSERT(
		resolution.BothAboveZero(), "Cannot create render target with an invalid resolution"
	);

	render_target = impl::AddRenderTargetComponents(
		render_target, manager, resolution, clear_color, texture_format
	);

	PTGN_ASSERT(render_target);

	return render_target;
}

RenderTarget CreateRenderTarget(
	Manager& manager, const V2_int& size, const Color& clear_color, TextureFormat texture_format
) {
	auto render_target{ impl::AddRenderTargetComponents(
		manager.CreateEntity(), manager, size, clear_color, texture_format
	) };
	return render_target;
}

} // namespace ptgn