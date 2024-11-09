#include "scene/scene.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

#include "core/game.h"
#include "math/vector2.h"
#include "renderer/render_texture.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"
#include "utility/log.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

SceneTransition::SceneTransition(TransitionType type, milliseconds duration) :
	type_{ type }, duration_{ duration } {}

bool SceneTransition::operator==(const SceneTransition& o) const {
	return type_ == o.type_ && duration_ == o.duration_;
}

bool SceneTransition::operator!=(const SceneTransition& o) const {
	return !(*this == o);
}

SceneTransition& SceneTransition::SetDuration(milliseconds duration) {
	PTGN_ASSERT(duration > milliseconds{ 0 }, "Cannot set scene transition duration <= 0");
	duration_ = duration;
	return *this;
}

SceneTransition& SceneTransition::SetColorFadeFraction(float color_fade_fraction) {
	PTGN_ASSERT(color_fade_fraction >= 0.0f, "Invalid color fade fraction");
	PTGN_ASSERT(color_fade_fraction <= 0.5f, "Invalid color fade fraction");
	color_start_fraction_ = color_fade_fraction;
	return *this;
}

SceneTransition& SceneTransition::SetType(TransitionType type) {
	type_ = type;
	return *this;
}

SceneTransition& SceneTransition::SetFadeThroughColor(const Color& color) {
	fade_through_color_ = color;
	return *this;
}

void SceneTransition::Start(
	bool transition_in, std::size_t key, std::size_t other_key, const std::shared_ptr<Scene>& scene
) const {
	PTGN_ASSERT(type_ != TransitionType::None);

	Tween tween{ duration_ };
	auto target{ scene->target_ };
	auto camera{ target.GetCamera() };
	auto clear_color{ target.GetClearColor() };

	std::function<void(float)> update;
	std::function<void()> start;
	std::function<void()> stop;

	V2_float s{ target.GetSize() };
	// Camera starting position.
	V2_float c{ camera.GetPosition() };
	const V2_float og_c{ c };
	const std::uint8_t og_opacity{ target.GetOpacity() };

	const auto push = [&](const V2_float& dir) {
		if (transition_in) {
			c	  -= s * dir;
			start  = [=]() mutable {
				 camera.SetPosition(c);
			};
		}
		update = [=](float f) mutable {
			camera.SetPosition(c + s * f * dir);
		};
	};
	const auto uncover = [&](const V2_float& dir) {
		if (transition_in) {
			start = [=]() {
				game.scene.SwitchActiveScenesImpl(key, other_key);
			};
			return;
		}
		update = [=](float f) mutable {
			camera.SetPosition(c + s * f * dir);
		};
	};
	const auto cover = [&](const V2_float& dir) {
		if (!transition_in) {
			return;
		}
		c	  -= s * dir;
		start  = [=]() mutable {
			 camera.SetPosition(c);
		};
		update = [=](float f) mutable {
			camera.SetPosition(c + s * f * dir);
		};
	};

	const auto fade = [&]() {
		if (transition_in) {
			start = [=]() mutable {
				target.SetOpacity(0);
			};
			update = [=](float f) mutable {
				target.SetOpacity(static_cast<std::uint8_t>(255.0f * f));
			};
		} else {
			start = [=]() mutable {
				target.SetOpacity(255);
			};
			update = [=](float f) mutable {
				target.SetOpacity(static_cast<std::uint8_t>(255.0f * (1.0f - f)));
			};
		}
	};
	const auto fade_through_color = [&]() {
		float start_frac{ color_start_fraction_ };
		Color fade_color{ fade_through_color_ };
		// TODO: Fix fade through color setting not working.
		if (transition_in) {
			start = [=]() mutable {
				target.SetOpacity(0);
			};
			update = [=](float f) mutable {
				target.SetClearColor(fade_color);
				if (f >= 1.0f - start_frac) {
					float p{ (f - (1.0f - start_frac)) / start_frac };
					target.SetOpacity(static_cast<std::uint8_t>(255.0f * p));
				} else {
					target.SetOpacity(0);
				}
			};
		} else {
			start = [=]() mutable {
				target.SetOpacity(255);
			};
			update = [=](float f) mutable {
				target.SetClearColor(fade_color);
				if (f <= start_frac) {
					float p{ 1.0f - f / start_frac };
					target.SetOpacity(static_cast<std::uint8_t>(255.0f * p));
				} else {
					target.SetOpacity(0);
				}
			};
		}
	};

	switch (type_) {
		case TransitionType::Custom:
			if (transition_in) {
				start  = start_in;
				update = update_in;
				stop   = stop_in;
			} else {
				start  = start_out;
				update = update_out;
				stop   = stop_out;
			}
			break;
		case TransitionType::UncoverDown:	   uncover({ 0, 1 }); break;
		case TransitionType::UncoverUp:		   uncover({ 0, -1 }); break;
		case TransitionType::UncoverLeft:	   uncover({ -1, 0 }); break;
		case TransitionType::UncoverRight:	   uncover({ 1, 0 }); break;
		case TransitionType::Fade:			   fade(); break;
		case TransitionType::FadeThroughColor: fade_through_color(); break;
		case TransitionType::PushDown:		   push({ 0, 1 }); break;
		case TransitionType::PushUp:		   push({ 0, -1 }); break;
		case TransitionType::PushLeft:		   push({ -1, 0 }); break;
		case TransitionType::PushRight:		   push({ 1, 0 }); break;
		case TransitionType::CoverDown:		   cover({ 0, 1 }); break;
		case TransitionType::CoverUp:		   cover({ 0, -1 }); break;
		case TransitionType::CoverLeft:		   cover({ -1, 0 }); break;
		case TransitionType::CoverRight:	   cover({ 1, 0 }); break;
		default:							   PTGN_ERROR("Invalid transition type");
	}
	tween.OnStart([=](float f) {
		// Important that this add active happens before start is invoked as in the case of the
		// uncover transition, start will change the order of the active scenes to ensure that the
		// new active scenes is not rendered on top of the covering scenes.
		if (transition_in) {
			game.scene.AddActiveImpl(key);
		}
		if (start != nullptr) {
			std::invoke(start);
		}
	});
	if (!transition_in) {
		tween.OnComplete([=]() { game.scene.RemoveActiveImpl(key); });
	}
	tween.OnUpdate([=](float f) {
		if (update != nullptr) {
			std::invoke(update, f);
		}
	});
	tween.OnDestroy([=]() mutable {
		camera.SetPosition(og_c);
		target.SetClearColor(clear_color);
		target.SetOpacity(og_opacity);
		if (stop != nullptr) {
			std::invoke(stop);
		}
	});
	game.tween.Add(tween).Start();
}

Scene::Scene() {
	target_ = RenderTexture{ true, color::Transparent, BlendMode::Blend };
}

void Scene::Add(Action new_status) {
	actions_.insert(new_status);
}

} // namespace ptgn