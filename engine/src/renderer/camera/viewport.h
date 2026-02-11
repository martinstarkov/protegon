#pragma once

#include "core/math/vector2.h"
#include "serialization/json/enum.h"

namespace ptgn {

struct Transform;

enum class ViewportType {
	Game,
	Display,
	World,
	WindowCenter,
	WindowTopLeft
};

PTGN_SERIALIZE_ENUM(
	ViewportType, { { ViewportType::Game, "game" },
					{ ViewportType::Display, "display" },
					{ ViewportType::World, "world" },
					{ ViewportType::WindowCenter, "window_center" },
					{ ViewportType::WindowTopLeft, "window_top_left" } }
);

/*
[[nodiscard]] V2_float DisplayToGame(V2_float game_scale, V2_float display_point);
[[nodiscard]] V2_float DisplayToWorld(
	V2_float game_scale, const Transform& rt_transform, V2_float display_point,
	const Camera& camera
);

[[nodiscard]] V2_float GameToDisplay(V2_float game_scale, V2_float game_point);
[[nodiscard]] V2_float GameToWorld(
	const Transform& rt_transform, V2_float game_point, const Camera& camera
);

[[nodiscard]] V2_float CameraToWorld(V2_float scene_point, const Camera& camera);
[[nodiscard]] V2_float CameraToDisplay(
	V2_float game_scale, V2_float game_size, V2_float scene_point,
	const Camera& camera
);
[[nodiscard]] V2_float CameraToGame(
	V2_float game_size, V2_float scene_point, const Camera& camera
);

[[nodiscard]] V2_float WorldToDisplay(
	V2_float game_scale, V2_float game_size, V2_float world_point,
	const Camera& camera
);
[[nodiscard]] V2_float WorldToGame(
	V2_float game_size, V2_float world_point, const Camera& camera
);
[[nodiscard]] V2_float WorldToCamera(V2_float world_point, const Camera& camera);

namespace impl {

// The window is an internal engine concept not exposed to the user directly.

[[nodiscard]] V2_float WindowToDisplay(V2_float window_point);
[[nodiscard]] V2_float WindowToGame(V2_float game_scale, V2_float window_point);
[[nodiscard]] V2_float DisplayToWindow(
	V2_float window_size, V2_float display_size, V2_float display_point
);
[[nodiscard]] V2_float GameToWindow(
	V2_float window_size, V2_float display_size, V2_float game_scale,
	V2_float game_point
);
[[nodiscard]] V2_float WindowToSceneTarget(
	V2_float game_scale, const Transform& rt_transform, V2_float window_point
);
[[nodiscard]] V2_float DisplayToSceneTarget(
	V2_float game_scale, const Transform& rt_transform, V2_float display_point
);
[[nodiscard]] V2_float GameToSceneTarget(const Transform& rt_transform, V2_float game_point);
[[nodiscard]] V2_float CameraToWindow(
	V2_float window_size, V2_float display_size, V2_float game_scale,
	V2_float game_size, V2_float scene_point, const Camera& camera
);
[[nodiscard]] V2_float WindowToWorld(
	V2_float game_scale, const Transform& rt_transform, V2_float window_point,
	const Camera& camera
);
[[nodiscard]] V2_float WorldToWindow(
	V2_float window_size, V2_float display_size, V2_float game_scale,
	V2_float game_size, V2_float world_point, const Camera& camera
);

} // namespace impl
*/

} // namespace ptgn