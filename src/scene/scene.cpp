#include "scene/scene.h"

#include <chrono>
#include <memory>

#include "core/game.h"
#include "math/geometry/polygon.h"
#include "renderer/color.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"
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

SceneTransition& SceneTransition::SetFadeColorDuration(milliseconds duration) {
	color_duration_ = duration;
	return *this;
}

SceneTransition& SceneTransition::SetType(TransitionType type) {
	type_ = type;
	return *this;
}

SceneTransition& SceneTransition::SetFadeColor(const Color& color) {
	fade_color_ = color;
	return *this;
}

void SceneTransition::Start(const std::shared_ptr<Scene>& scene) const {
	if (type_ == TransitionType::None) {
		scene->Add(Scene::Action::Enter);
		if (!game.scene.transition_queue_.empty()) {
			game.scene.transition_queue_.pop_front();
		}
		return;
	}

	auto start_next_scene = []() {
		if (!game.scene.transition_queue_.empty()) {
			game.scene.transition_queue_.pop_front();
		}
		if (!game.scene.transition_queue_.empty()) {
			auto& front{ game.scene.transition_queue_.front() };
			front.transition.Start(front.scene);
		}
	};

	// While it may sound good, transitioning a scene to itself can cause many repeated scene
	// transitions and other bugs.
	if (scene == game.scene.current_scene_) {
		std::invoke(start_next_scene);
		return;
	}

	Tween tween;

	// TODO: Remove temporary:
	PTGN_ASSERT(
		type_ == TransitionType::FadeThroughColor, "Other scene transitions currently not supported"
	);

	std::function<void(float)> update;
	std::function<void()> start;
	std::function<void()> stop;

	const auto fade_through_color = [&]() {
		milliseconds fade_half_duration{ duration_ / 2 };
		Color fade_color{ fade_color_ };
		auto fade = [=](float f) {
			Color c{ fade_color };
			c.a = static_cast<std::uint8_t>(255.0f * f);
			Rect::Fullscreen().Draw(c, -1.0f, std::numeric_limits<std::int32_t>::infinity());
		};
		if (game.scene.HasCurrent()) {
			tween.During(fade_half_duration)
				.OnUpdate(fade)
				.During(color_duration_)
				.OnUpdate([=]() {
					Rect::Fullscreen().Draw(
						fade_color, -1.0f, std::numeric_limits<std::int32_t>::infinity()
					);
				})
				.During(fade_half_duration)
				.OnStart([=]() mutable { scene->Add(Scene::Action::Enter); })
				.Reverse()
				.OnUpdate(fade);
		} else {
			// If transitioning into the starting scene, skip the entire solid color part of the
			// fade.
			tween.During(duration_)
				.OnStart([=]() mutable { scene->Add(Scene::Action::Enter); })
				.Reverse()
				.OnUpdate(fade);
		}
	};
	/*
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
		float start_alpha{ static_cast<float>(scene->tint_.a) };
		if (transition_in) {
			start = [=]() mutable {
				scene->tint_.a = 0;
			};
			update = [=](float f) mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha * f);
			};
			stop = [=]() mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
			};
		} else {
			start = [=]() mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
			};
			update = [=](float f) mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha * (1.0f - f));
			};
			stop = [=]() mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
			};
		}
	};*/

	switch (type_) {
		case TransitionType::Custom:
			// TODO: Implement.
			break;
		case TransitionType::FadeThroughColor: std::invoke(fade_through_color); break;
		/*
		case TransitionType::UncoverDown:	   uncover({ 0, 1 }); break;
		case TransitionType::UncoverUp:		   uncover({ 0, -1 }); break;
		case TransitionType::UncoverLeft:	   uncover({ -1, 0 }); break;
		case TransitionType::UncoverRight:	   uncover({ 1, 0 }); break;
		case TransitionType::Fade:			   fade(); break;
		case TransitionType::PushDown:		   push({ 0, 1 }); break;
		case TransitionType::PushUp:		   push({ 0, -1 }); break;
		case TransitionType::PushLeft:		   push({ -1, 0 }); break;
		case TransitionType::PushRight:		   push({ 1, 0 }); break;
		case TransitionType::CoverDown:		   cover({ 0, 1 }); break;
		case TransitionType::CoverUp:		   cover({ 0, -1 }); break;
		case TransitionType::CoverLeft:		   cover({ -1, 0 }); break;
		case TransitionType::CoverRight:	   cover({ 1, 0 }); break;
		*/
		case TransitionType::None:			   [[fallthrough]];
		default:							   PTGN_ERROR("Invalid transition type");
	}
	tween.During(milliseconds{ 0 }).OnComplete([&]() { std::invoke(start_next_scene); });
	game.tween.Add(tween).Start();
}

Scene::Scene() {}

void Scene::Add(Action new_action) {
	actions_.insert(new_action);
}

void Scene::Remove(Action action) {
	actions_.erase(action);
}

bool Scene::HasActions() const {
	return !actions_.empty();
}

} // namespace ptgn
