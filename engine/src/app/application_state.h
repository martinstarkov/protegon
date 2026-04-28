#pragma once

#include <cstdint>
#include <utility>

namespace ptgn {

enum class ApplicationState {
	/// @brief Initial asset loading.
	Launching,
	/// @brief Normal scene update + render.
	Running,
	/// @brief Paused state, no updates, rendering, or events.
	Paused,
	/// @brief Render scenes without scene simulation updates.
	RenderOnly,
	/// @brief Update scenes without rendering/presenting.
	UpdateOnly,
};

namespace impl {

enum class ApplicationFeature : std::uint32_t {
	None				= 0,
	UpdateScenes		= 1 << 0,
	RenderScenes		= 1 << 1,
	DispatchSceneEvents = 1 << 2,

	All = UpdateScenes | RenderScenes | DispatchSceneEvents
};

constexpr ApplicationFeature operator|(ApplicationFeature a, ApplicationFeature b) {
	return static_cast<ApplicationFeature>(std::to_underlying(a) | std::to_underlying(b));
}

constexpr ApplicationFeature operator&(ApplicationFeature a, ApplicationFeature b) {
	return static_cast<ApplicationFeature>(std::to_underlying(a) & std::to_underlying(b));
}

constexpr ApplicationFeature operator~(ApplicationFeature value) {
	return static_cast<ApplicationFeature>(~std::to_underlying(value));
}

constexpr ApplicationFeature& operator|=(ApplicationFeature& a, ApplicationFeature b) {
	a = a | b;
	return a;
}

constexpr ApplicationFeature& operator&=(ApplicationFeature& a, ApplicationFeature b) {
	a = a & b;
	return a;
}

constexpr bool ApplicationHasFeature(ApplicationFeature features, ApplicationFeature feature) {
	return std::to_underlying(features & feature) != 0;
}

constexpr ApplicationFeature ApplicationFeaturesForState(ApplicationState state) {
	using enum ApplicationFeature;

	switch (state) {
		using enum ApplicationState;
		case Running:	 return UpdateScenes | RenderScenes | DispatchSceneEvents;
		case RenderOnly: return RenderScenes;
		case UpdateOnly: return UpdateScenes | DispatchSceneEvents;
		case Paused:	 return RenderScenes;
		case Launching:	 return None;
	}

	return None;
}

} // namespace impl

} // namespace ptgn