// #include "renderer/render_target.h"
//
// #include <functional>
// #include <string_view>
// #include <vector>
//
// #include "core/app/application.h"
// #include "core/app/manager.h"
// #include "core/assert.h"
// #include "ecs/components/draw.h"
// #include "ecs/components/sprite.h"
// #include "ecs/components/transform.h"
// #include "ecs/entity.h"
// #include "ecs/game_object.h"
// #include "core/log.h"
// #include "core/scripting/script.h"
// #include "core/scripting/script_interfaces.h"
// #include "math/vector2.h"
// #include "renderer/api/color.h"
// #include "renderer/buffer/frame_buffer.h"
//
// #include "renderer/render_data.h"
// #include "renderer/renderer.h"
// #include "world/scene/camera.h"
//
// namespace ptgn {
//
// namespace impl {
//
// RenderTarget AddRenderTargetComponents(
//	const Entity& entity, Manager& manager, const V2_int& render_target_size, bool game_size_camera,
//	const Color& clear_color, TextureFormat texture_format
//) {
//	PTGN_ASSERT(entity);
//
//	RenderTarget render_target{ entity };
//
//	SetPosition(render_target, {});
//
//	render_target.Add<TextureHandle>();
//	render_target.Add<impl::DisplayList>();
//	render_target.Add<impl::ClearColor>(clear_color);
//	auto& camera{ render_target.Add<GameObject<Camera>>(CreateCamera(manager)) };
//	if (!game_size_camera) {
//		camera.SetViewport({}, render_target_size);
//	}
//	SetDraw<RenderTarget>(render_target);
//	Show(render_target);
//
//	// TODO: Move frame buffer object to a FrameBufferManager.
//	const auto& frame_buffer{ render_target.Add<impl::FrameBuffer>(
//		impl::Texture{ nullptr, render_target_size, texture_format }, true
//	) };
//
//	PTGN_ASSERT(frame_buffer.IsValid(), "Failed to create valid frame buffer for render target");
//	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind frame buffer for render target");
//
//	render_target.Clear();
//
//	return render_target;
// }
//
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
// namespace impl {
//
// RenderTarget& SetDrawFilterImpl(RenderTarget& render_target, std::string_view filter_name) {
//	EntityAccess::Add<IDrawFilter>(render_target, filter_name);
//	return render_target;
// }
//
// } // namespace impl
//
// bool RenderTarget::HasDrawFilter() const {
//	return Has<impl::IDrawFilter>();
// }
//
// RenderTarget& RenderTarget::RemoveDrawFilter() {
//	impl::EntityAccess::Remove<impl::IDrawFilter>(*this);
//	return *this;
// }
//
// RenderTarget CreateRenderTarget(
//	Manager& manager, ResizeMode resize_to_resolution, bool game_size_camera,
//	const Color& clear_color, TextureFormat texture_format
//) {
//	RenderTarget render_target{ manager.CreateEntity() };
//
//	V2_int resolution;
//
//	if (resize_to_resolution == ResizeMode::DisplaySize) {
//		resolution = Application::Get().render_.GetDisplaySize();
//		AddScript<impl::DisplayResizeScript>(render_target);
//	} else if (resize_to_resolution == ResizeMode::GameSize) {
//		resolution = Application::Get().render_.GetGameSize();
//		AddScript<impl::GameResizeScript>(render_target);
//	} else {
//		PTGN_ERROR("Unknown resize to resolution value");
//	}
//
//	PTGN_ASSERT(
//		resolution.BothAboveZero(), "Cannot create render target with an invalid resolution"
//	);
//
//	render_target = impl::AddRenderTargetComponents(
//		render_target, manager, resolution, game_size_camera, clear_color, texture_format
//	);
//
//	PTGN_ASSERT(render_target);
//
//	return render_target;
// }
//
// RenderTarget CreateRenderTarget(
//	Manager& manager, const V2_int& size, const Color& clear_color, TextureFormat texture_format,
//	bool game_size_camera
//) {
//	auto render_target{ impl::AddRenderTargetComponents(
//		manager.CreateEntity(), manager, size, game_size_camera, clear_color, texture_format
//	) };
//	return render_target;
// }
//
// } // namespace ptgn