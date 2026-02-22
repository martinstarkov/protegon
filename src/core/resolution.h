#pragma once

#include "math/vector2.h"
#include "scene/camera.h"

namespace ptgn {

// How the game size is scaled to the window size, resulting in display size.
enum class ScalingMode {
	Disabled,	  /* There is no scaling in effect */
	Stretch,	  /* The rendered content is stretched to the window size */
	Letterbox,	  /* The rendered content is fit to the largest dimension and the other dimension is
					 letterboxed with black bars */
	Overscan,	  /* The rendered content is fit to the smallest dimension and the other dimension
					 extends beyond the window bounds */
	IntegerScale, /* The rendered content is scaled up by integer multiples to fit the window size
				   */
};

enum class ViewportType {
	Game,
	Display,
	World,
	WindowCenter,
	WindowTopLeft
};

[[nodiscard]] V2_float DisplayToGame(const V2_float& display_point);
[[nodiscard]] V2_float DisplayToWorld(const V2_float& display_point, const Camera& camera = {});

[[nodiscard]] V2_float GameToDisplay(const V2_float& game_point);
[[nodiscard]] V2_float GameToWorld(const V2_float& game_point, const Camera& camera = {});

[[nodiscard]] V2_float CameraToWorld(const V2_float& scene_point, const Camera& camera = {});
[[nodiscard]] V2_float CameraToDisplay(const V2_float& scene_point, const Camera& camera = {});
[[nodiscard]] V2_float CameraToGame(const V2_float& scene_point, const Camera& camera = {});

[[nodiscard]] V2_float WorldToDisplay(const V2_float& world_point, const Camera& camera = {});
[[nodiscard]] V2_float WorldToGame(const V2_float& world_point, const Camera& camera = {});
[[nodiscard]] V2_float WorldToCamera(const V2_float& world_point, const Camera& camera = {});

namespace impl {

// The window is an internal engine concept not exposed to the user directly.

[[nodiscard]] V2_float WindowToDisplay(const V2_float& window_point);
[[nodiscard]] V2_float WindowToGame(const V2_float& window_point);
[[nodiscard]] V2_float DisplayToWindow(const V2_float& display_point);
[[nodiscard]] V2_float GameToWindow(const V2_float& game_point);
[[nodiscard]] V2_float WindowToSceneTarget(const V2_float& window_point);
[[nodiscard]] V2_float DisplayToSceneTarget(const V2_float& display_point);
[[nodiscard]] V2_float GameToSceneTarget(const V2_float& game_point);
[[nodiscard]] V2_float CameraToWindow(const V2_float& scene_point, const Camera& camera);
[[nodiscard]] V2_float WindowToWorld(const V2_float& window_point, const Camera& camera);
[[nodiscard]] V2_float WorldToWindow(const V2_float& world_point, const Camera& camera);

} // namespace impl

} // namespace ptgn