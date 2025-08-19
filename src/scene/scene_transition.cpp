#include "scene/scene_transition.h"

#include "tweens/tween.h"

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
	bool transition_in, std::size_t key, std::size_t other_key, Scene* scene
) const {
	/*
	if (type_ == TransitionType::None) {
		if (transition_in) {
			game.scene.AddActiveImpl(
				key, game.scene.active_scenes_.empty() && game.scene.Size() == 1
			);
		} else {
			game.scene.RemoveActiveImpl(key);
		}
		return;
	}

	Tween tween{ CreateTween(*scene) };
	if (type_ == TransitionType::FadeThroughColor) {
		tween = Tween{ duration_ + color_duration_ };
	} else {
		tween = Tween{ duration_ };
	}

	RenderTarget target{ scene->target_ };
	OrthographicCamera camera{ target.GetCamera().GetPrimary() };

	std::function<void(float)> update;
	std::function<void()> start;
	std::function<void()> stop;

	V2_float s{ target.GetTexture().GetSize() };
	// Camera starting position.
	V2_float c{ camera.GetPosition() };
	const V2_float og_c{ c };

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
	};
	const auto fade_through_color = [&]() {
		float transition_duration{
			to_duration_value<millisecondsf>(duration_)
		};
		float color_duration{
			to_duration_value<millisecondsf>(color_duration_)
		};
		float total_duration{ transition_duration + color_duration };
		float fade_duration{ transition_duration / 2.0f };
		float start_color_frac{ fade_duration / total_duration };
		float stop_color_frac{ (fade_duration + color_duration) / total_duration };
		PTGN_ASSERT(start_color_frac != 1.0f, "Invalid fade through color start duration");
		PTGN_ASSERT(stop_color_frac != 1.0f, "Invalid fade through color stop duration");
		float start_alpha{ static_cast<float>(scene->tint_.a) };
		Color fade_color{ fade_color_ };
		if (transition_in) {
			start = [=]() mutable {
				scene->tint_.a = 0;
			};
			update = [=](float f) mutable {
				Color c{ fade_color };

				if (f <= start_color_frac) {
					return;
				} else if (f >= stop_color_frac) {
					scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
					float renormalized{ ((1.0f - f) / (1.0f - stop_color_frac)) };
					c.a = static_cast<std::uint8_t>(255.0f * renormalized);
				}

				Rect::Fullscreen().Draw(
					c, -1.0f, { std::numeric_limits<std::int32_t>::infinity() }
				);
			};
			stop = [=]() mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
			};
		} else {
			start = [=]() mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
			};
			update = [=](float f) mutable {
				if (f <= start_color_frac) {
					float renormalized{ 1.0f - f / start_color_frac };
					Color c{ fade_color };
					c.a = static_cast<std::uint8_t>(255.0f * (1.0f - renormalized));
					Rect::Fullscreen().Draw(
						c, -1.0f, { std::numeric_limits<std::int32_t>::infinity() }
					);
				} else {
					scene->tint_.a = 0;
				}
			};
			stop = [=]() mutable {
				scene->tint_.a = static_cast<std::uint8_t>(start_alpha);
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
		case TransitionType::None:			   [[fallthrough]];
		default:							   PTGN_ERROR("Invalid transition type");
	}
	tween.OnStart([=]([[maybe_unused]] float f) {
		// Important that this add active happens before start is invoked as in the case of the
		// uncover transition, start will change the order of the active scenes to ensure that the
		// new active scenes is not rendered on top of the covering scenes.
		if (transition_in) {
			game.scene.AddActiveImpl(
				key, game.scene.active_scenes_.empty() && game.scene.Size() == 1
			);
		}
		if (start != nullptr) {
			start();
		}
	});
	if (!transition_in) {
		tween.OnComplete([=]() { game.scene.RemoveActiveImpl(key); });
	}
	tween.OnUpdate([=](float f) {
		if (update != nullptr) {
			update(f);
		}
	});
	tween.OnDestroy([=]() mutable {
		camera.SetPosition(og_c);
		if (stop != nullptr) {
			stop();
		}
	});
	game.tween.Add(tween).Start();
	*/
}

} // namespace ptgn