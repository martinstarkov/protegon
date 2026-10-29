#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

enum class SceneTransitionStyle : std::uint8_t {
	None,
	Fade,
	CrossFade,
	Slide
};

enum class SceneChangeAction : std::uint8_t {
	/// Enters an inactive scene or reconstructs it if it is already active.
	Enter,
	Exit,
	Switch
};

struct SceneChangeScript : public Script {
	SceneChangeAction action{ SceneChangeAction::Switch };
	std::string scene_key{ "main" };
	SceneTransitionStyle transition{ SceneTransitionStyle::None };
	milliseconds duration{ 500 };
	milliseconds delay{ 0 };
	Ease ease{ Ease::Linear };
	V2_float direction{ 1.0f, 0.0f };
	std::size_t priority{ 0 };
	void OnStart() override;
	PTGN_REFLECT(
		SceneChangeScript, action, scene_key, transition, duration, delay, ease, direction, priority
	)
};

struct TilemapSetTileScript : public Script {
	void OnStart() override;
	TilemapTile tile{};

	PTGN_REFLECT(TilemapSetTileScript, tile)
};

struct TilemapRemoveTileScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(TilemapRemoveTileScript, vector)
};

} // namespace ptgn
