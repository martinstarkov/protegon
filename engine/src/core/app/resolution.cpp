#include "core/app/resolution.h"

#include "core/assert.h"
#include "ecs/components/transform.h"
#include "math/vector2.h"

namespace ptgn {

/*
V2_float DisplayToGame(const V2_float& game_scale, const V2_float& display_point) {
PTGN_ASSERT(game_scale.BothAboveZero());
auto game_point{ display_point / game_scale };
return game_point;
}

V2_float GameToDisplay(const V2_float& game_scale, const V2_float& game_point) {
PTGN_ASSERT(game_scale.BothAboveZero());
auto display_point{ game_point * game_scale };
return display_point;
}

V2_float DisplayToWorld(
const V2_float& game_scale, const Transform& rt_transform, const V2_float& display_point,
const Camera& camera
) {
auto game_point{ DisplayToGame(game_scale, display_point) };
auto world_point{ GameToWorld(rt_transform, game_point, camera) };
return world_point;
}

V2_float WorldToDisplay(
const V2_float& game_scale, const V2_float& game_size, const V2_float& world_point,
const Camera& camera
) {
auto game_point{ WorldToGame(game_size, world_point, camera) };
auto display_point{ GameToDisplay(game_scale, game_point) };
return display_point;
}

V2_float GameToWorld(
const Transform& rt_transform, const V2_float& game_point, const Camera& camera
) {
auto camera_point{ impl::GameToSceneTarget(rt_transform, game_point) };
auto world_point{ CameraToWorld(camera_point, camera) };
return world_point;
}

V2_float WorldToGame(const V2_float& game_size, const V2_float& world_point, const Camera& camera) {
auto camera_point{ WorldToCamera(world_point, camera) };
auto game_point{ CameraToGame(game_size, camera_point, camera) };
return game_point;
}

V2_float CameraToWorld(const V2_float& camera_point, const Camera& camera) {
PTGN_ASSERT(camera);
Transform camera_transform{ camera.GetTransform() };
auto world_point{ camera_transform.Apply(camera_point) };

return world_point;
}

V2_float WorldToCamera(const V2_float& world_point, const Camera& camera) {
PTGN_ASSERT(camera);
Transform camera_transform{ camera.GetTransform() };
auto camera_point{ camera_transform.ApplyInverse(world_point) };

return camera_point;
}

V2_float CameraToDisplay(
const V2_float& game_scale, const V2_float& game_size, const V2_float& camera_point,
const Camera& camera
) {
auto game_point{ CameraToGame(game_size, camera_point, camera) };
auto display_point{ GameToDisplay(game_scale, game_point) };
return display_point;
}

V2_float CameraToGame(
const V2_float& game_size, const V2_float& camera_point, const Camera& camera
) {
PTGN_ASSERT(camera);

auto camera_viewport_pos{ camera.GetViewportPosition() };
auto camera_viewport_size{ camera.GetViewportSize() };
PTGN_ASSERT(camera_viewport_size.BothAboveZero());
PTGN_ASSERT(game_size.BothAboveZero());

V2_float game_point{ (camera_point - camera_viewport_pos) / camera_viewport_size * game_size };

return game_point;
}

namespace impl {

V2_float WindowToDisplay(const V2_float& window_point) {
auto window_size{ window.GetSize() };
auto display_size{ render.GetDisplaySize() };

V2_float offset{ (window_size - display_size) * 0.5f };

auto display_point{ window_point - offset };

return window_point;
}

V2_float DisplayToWindow(
const V2_float& window_size, const V2_float& display_size, const V2_float& display_point
) {
V2_float offset{ (window_size - display_size) * 0.5f };

V2_float window_point{ display_point + offset };
return window_point;
}

V2_float WindowToGame(const V2_float& game_scale, const V2_float& window_point) {
auto display_point{ WindowToDisplay(window_point) };
auto game_point{ DisplayToGame(game_scale, display_point) };
return game_point;
}

V2_float GameToWindow(
const V2_float& window_size, const V2_float& display_size, const V2_float& game_scale,
const V2_float& game_point
) {
auto display_point{ GameToDisplay(game_scale, game_point) };
auto window_point{ DisplayToWindow(window_size, display_size, display_point) };
return window_point;
}

V2_float WindowToSceneTarget(
const V2_float& game_scale, const Transform& rt_transform, const V2_float& window_point
) {
auto display_point{ WindowToDisplay(window_point) };
auto camera_point{ DisplayToSceneTarget(game_scale, rt_transform, display_point) };
return camera_point;
}

V2_float CameraToWindow(
const V2_float& window_size, const V2_float& display_size, const V2_float& game_scale,
const V2_float& game_size, const V2_float& camera_point, const Camera& camera
) {
auto display_point{ CameraToDisplay(game_scale, game_size, camera_point, camera) };
auto window_point{ DisplayToWindow(window_size, display_size, display_point) };
return window_point;
}

V2_float WindowToWorld(
const V2_float& game_scale, const Transform& rt_transform, const V2_float& window_point,
const Camera& camera
) {
auto display_point{ WindowToDisplay(window_point) };
auto world_point{ DisplayToWorld(game_scale, rt_transform, display_point, camera) };
return world_point;
}

V2_float WorldToWindow(
const V2_float& window_size, const V2_float& display_size, const V2_float& game_scale,
const V2_float& game_size, const V2_float& world_point, const Camera& camera
) {
auto display_point{ WorldToDisplay(game_scale, game_size, world_point, camera) };
auto window_point{ DisplayToWindow(window_size, display_size, display_point) };
return window_point;
}

V2_float DisplayToSceneTarget(
const V2_float& game_scale, const Transform& rt_transform, const V2_float& display_point
) {
auto game_point{ DisplayToGame(game_scale, display_point) };
auto camera_point{ GameToSceneTarget(rt_transform, game_point) };
return camera_point;
}

V2_float GameToSceneTarget(const Transform& rt_transform, const V2_float& game_point) {
auto position{ rt_transform.GetPosition() };
auto scale{ rt_transform.GetScale() };
auto rotation{ rt_transform.GetRotation() };

PTGN_ASSERT(
	scale.BothAboveZero(), "Cannot transform screen to scene with zero or negative scale"
);

auto camera_point{ ((game_point - position) / scale).Rotated(-rotation) };

return camera_point;
}

} // namespace impl

*/

} // namespace ptgn
