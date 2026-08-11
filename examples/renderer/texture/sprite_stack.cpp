#include <algorithm>
#include <cmath>
#include <numbers>

#include "app/application.h"
#include "app/editor.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/angle.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"

using namespace ptgn;

namespace {

constexpr float kMaximumForwardSpeed{ 350.0f };
constexpr float kMaximumReverseSpeed{ 190.0f };

constexpr float kForwardAcceleration{ 250.0f };
constexpr float kReverseAcceleration{ 230.0f };

constexpr float kBrakingDeceleration{ 480.0f };
constexpr float kRollingDeceleration{ 400.0f };

constexpr float kSteeringRate{
	150.0f * std::numbers::pi_v<float> / 180.0f
};

constexpr float kLowSpeedSteering{ 0.20f };

constexpr float kMaximumPhysicsDeltaTime{ 0.05f };

float MoveToward(float value, float target, float maximum_delta) {
	if (value < target) {
		return std::min(value + maximum_delta, target);
	}
	if (value > target) {
		return std::max(value - maximum_delta, target);
	}
	return target;
}

void NormalizeHeading(Radians& heading) {
	constexpr float kFullRotation{
		2.0f * std::numbers::pi_v<float>
	};

	heading.value = std::fmod(heading.value, kFullRotation);

	if (heading.value < 0.0f) {
		heading.value += kFullRotation;
	}
}

void CreateTrack(Scene& scene) {
	Entity asphalt{
		CreateCircle(scene, { 0.0f, 0.0f }, 330.0f, Color{ 51, 54, 59, 255 })
	};
	SetDepth(asphalt, -20.0f);

	Entity grass_center{
		CreateCircle(scene, { 0.0f, 0.0f }, 205.0f, Color{ 47, 93, 55, 255 })
	};
	SetDepth(grass_center, -19.0f);

	constexpr int kMarkerCount{ 20 };

	for (int i{ 0 }; i < kMarkerCount; ++i) {
		const float angle{
			static_cast<float>(i) / static_cast<float>(kMarkerCount) * 2.0f *
			std::numbers::pi_v<float>
		};

		const V2_float position{
			std::sin(angle) * 267.0f,
			-std::cos(angle) * 267.0f,
		};

		Entity marker{
			CreateCircle(
				scene,
				position,
				5.0f,
				i % 2 == 0 ? Color{ 238, 224, 177, 255 } : Color{ 207, 79, 63, 255 }
			)
		};
		SetDepth(marker, -18.0f);
	}
}

} // namespace

class SpriteStackDrivingScene : public Scene {
public:
	void OnLoad() override {
		ctx().asset.Load({ { "car", "assets/sprite_stack_slices11.png" } });

		car_	= SpriteStack{ GetEntity(Tag{ "Car" }) };
	}

	void OnNew() override {
		SetBackgroundColor(Color{ 29, 65, 38, 255 });

		CreateTrack(*this);

		Transform car_transform;
		car_transform.position = { 0.0f, 210.0f };
		car_transform.rotation = heading_;
		car_transform.scale	= { 3.0f, 3.0f };

		car_ = CreateSpriteStack(
			*this,
			car_transform,
			"car",
			SpriteStackData{
				.layer_offset = { 0.0f, -1.2f },
			},
			Origin::Center
		);

		car_.Add<Tag>("Car");
		SetDepth(car_, 0.0f);
	}

	void OnUpdate() override {
		if (!car_) {
			return;
		}

		const float dt{
			std::min(ctx().dt().count(), kMaximumPhysicsDeltaTime)
		};

		const float throttle{
			(forward_ ? 1.0f : 0.0f) - (reverse_ ? 1.0f : 0.0f)
		};

		const float steering{
			(right_ ? 1.0f : 0.0f) - (left_ ? 1.0f : 0.0f)
		};

		UpdateSpeed(throttle, dt);
		UpdateSteering(steering, dt);
		UpdateCar(dt);
	}

	void OnEvent(Event event) override {
		event.Dispatch<event::KeyPressed>([this](const auto& input) {
			SetKey(input.key, true);
		});
		event.Dispatch<event::KeyHeld>([this](const auto& input) {
			SetKey(input.key, true);
		});
		event.Dispatch<event::KeyReleased>([this](const auto& input) {
			SetKey(input.key, false);
		});
	}

private:
	void UpdateSpeed(float throttle, float dt) {
		if (throttle > 0.0f) {
			if (speed_ < 0.0f) {
				speed_ = MoveToward(
					speed_,
					0.0f,
					kBrakingDeceleration * dt
				);
			} else {
				speed_ += kForwardAcceleration * dt;
			}
		} else if (throttle < 0.0f) {
			if (speed_ > 0.0f) {
				speed_ = MoveToward(
					speed_,
					0.0f,
					kBrakingDeceleration * dt
				);
			} else {
				speed_ -= kReverseAcceleration * dt;
			}
		} else {
			speed_ = MoveToward(
				speed_,
				0.0f,
				kRollingDeceleration * dt
			);
		}

		speed_ = std::clamp(
			speed_,
			-kMaximumReverseSpeed,
			kMaximumForwardSpeed
		);
	}

	void UpdateSteering(float steering, float dt) {
		if (steering == 0.0f || std::abs(speed_) <= 1.0f) {
			return;
		}

		const float speed_limit{
			speed_ >= 0.0f ? kMaximumForwardSpeed : kMaximumReverseSpeed
		};

		const float speed_fraction{
			std::min(
				1.0f,
				std::abs(speed_) / speed_limit
			)
		};

		const float steering_strength{
			kLowSpeedSteering +
			(1.0f - kLowSpeedSteering) * speed_fraction
		};

		const float travel_sign{
			speed_ >= 0.0f ? 1.0f : -1.0f
		};

		heading_.value +=
			steering *
			kSteeringRate *
			steering_strength *
			travel_sign *
			dt;

		NormalizeHeading(heading_);
	}

	void UpdateCar(float dt) {
		auto& transform{ car_.Get<Transform>() };

		transform.rotation = heading_;

		// This sprite-stack source faces right at rotation 0, so +X is the
		// vehicle's forward direction.
		const V2_float forward{
			std::cos(heading_.value),
			std::sin(heading_.value),
		};

		transform.position += forward * speed_ * dt;

		WrapPosition(transform.position);
	}

	void WrapPosition(V2_float& position) {
		constexpr V2_float kHalfBounds{ 430.0f, 330.0f };

		if (position.x < -kHalfBounds.x) {
			position.x = kHalfBounds.x;
		} else if (position.x > kHalfBounds.x) {
			position.x = -kHalfBounds.x;
		}

		if (position.y < -kHalfBounds.y) {
			position.y = kHalfBounds.y;
		} else if (position.y > kHalfBounds.y) {
			position.y = -kHalfBounds.y;
		}
	}

	void SetKey(Key key, bool down) {
		switch (key) {
			case Key::W:
			case Key::Up: forward_ = down; break;

			case Key::S:
			case Key::Down: reverse_ = down; break;

			case Key::A:
			case Key::Left: left_ = down; break;

			case Key::D:
			case Key::Right: right_ = down; break;

			default: break;
		}
	}

	SpriteStack car_;

	Radians heading_;
	float speed_{ 0.0f };

	bool forward_{ false };
	bool reverse_{ false };
	bool left_{ false };
	bool right_{ false };
};

PTGN_REGISTER_SCENE(SpriteStackDrivingScene, "Sprite Stack Driving Scene");

int main(int, char**) {
	Application app{
		"Sprite Stack Car Driving Demo",
		{ 960, 720 },
	};

	PTGN_WITH_EDITOR(app, true);

	app.StartProject<SpriteStackDrivingScene>(
		"SpriteStackDrivingProject/SpriteStackDriving.ptgnproj"
	);
}