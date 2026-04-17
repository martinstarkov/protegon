#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>

#include "runtime/ecs/manager.h"
// #include "runtime/graphics/animation.h"
// #include "runtime/graphics/draw.h"
// #include "runtime/graphics/interactive.h"
// #include "runtime/physics/lifetime.h"
// #include "runtime/graphics/movement.h"
// #include "runtime/graphics/offsets.h"
// #include "runtime/ecs/relatives.h"
// #include "runtime/graphics/sprite.h"
#include "core/log.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/util/timer.h"
#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"
// #include "physics/collider.h"
// #include "physics/rigid_body.h"
#include "core/math/geometry/origin.h"
#include "renderer/pipeline/blend_mode.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
// #include "renderer/render_target.h"
// #include "runtime/graphics/text/font.h"
// #include "runtime/graphics/text/text.h"
// #include "runtime/graphics/fx/light.h"
// #include "runtime/graphics/fx/particle.h"
// #include "tween/follow_config.h"
// #include "tween/shake_config.h"
// #include "tween/tween.h"
// #include "tween/tween_effect.h"
// #include "ui/button.h"
// #include "scene/camera.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

// #include "runtime/scene/scene_transition.h"

namespace ptgn::impl {

class ComponentRegistry {
public:
	using ComponentRegistrationFunc = void (*)(Manager& manager);

	static auto& GetData() {
		static std::unordered_map<std::size_t, ComponentRegistrationFunc> s;
		return s;
	}

	template <typename T>
	static bool Register() {
		// PTGN_LOG("Registering component: ", class_name);
		auto& registry{ GetData() };
		registry[Hash<T>()] = [](Manager& manager) {
			manager.template RegisterType<T>();
		};
		return true;
	}

	static void AddTypes(Manager& manager) {
		const auto& registry{ GetData() };
		for (const auto& [type_name, component_func] : registry) {
			component_func(manager);
		}
	}
};

} // namespace ptgn::impl

#define PTGN_REGISTER_COMPONENT(TComponent)                            \
	namespace ptgn::impl::registration {                               \
	struct TComponent##Registration {                                  \
		TComponent##Registration() {                                   \
			ptgn::impl::ComponentRegistry::Register<TComponent>();     \
		}                                                              \
	};                                                                 \
	static TComponent##Registration global_##TComponent##Registration; \
	}

// PTGN_REGISTER_COMPONENT(Visible)
// PTGN_REGISTER_COMPONENT(Tint)
// PTGN_REGISTER_COMPONENT(Depth)
//  PTGN_REGISTER_COMPONENT(LineWidth)
//  PTGN_REGISTER_COMPONENT(TextureCrop)
//  PTGN_REGISTER_COMPONENT(AnimationData)
// PTGN_REGISTER_COMPONENT(IDrawable)
// PTGN_REGISTER_COMPONENT(IDrawFilter)
// PTGN_REGISTER_COMPONENT(Draggable)
// PTGN_REGISTER_COMPONENT(Lifetime)
// PTGN_REGISTER_COMPONENT(Interactive)
//  PTGN_REGISTER_COMPONENT(TopDownMovement)
//  PTGN_REGISTER_COMPONENT(PlatformerMovement)
//  PTGN_REGISTER_COMPONENT(PlatformerJump)
// PTGN_REGISTER_COMPONENT(Offsets)
PTGN_REGISTER_COMPONENT(Transform)
PTGN_REGISTER_COMPONENT(UUID)
// PTGN_REGISTER_COMPONENT(ChildKey)
// PTGN_REGISTER_COMPONENT(SceneKey)
// PTGN_REGISTER_COMPONENT(Parent)
// PTGN_REGISTER_COMPONENT(Children)
PTGN_REGISTER_COMPONENT(Timer)
// PTGN_REGISTER_COMPONENT(Collider)
// PTGN_REGISTER_COMPONENT(RigidBody)
PTGN_REGISTER_COMPONENT(BlendMode)
PTGN_REGISTER_COMPONENT(Color)
PTGN_REGISTER_COMPONENT(Flip)
PTGN_REGISTER_COMPONENT(Origin)
// PTGN_REGISTER_COMPONENT(LightProperties)
// PTGN_REGISTER_COMPONENT(Particle)
// PTGN_REGISTER_COMPONENT(ParticleInfo)
// PTGN_REGISTER_COMPONENT(ParticleEmitterComponent)
// PTGN_REGISTER_COMPONENT(FontRenderMode)
// PTGN_REGISTER_COMPONENT(FontStyle)
// PTGN_REGISTER_COMPONENT(ClearColor)
// PTGN_REGISTER_COMPONENT(TextJustify)
// PTGN_REGISTER_COMPONENT(TextContent)
// PTGN_REGISTER_COMPONENT(FontSize)
// PTGN_REGISTER_COMPONENT(TextLineSkip)
// PTGN_REGISTER_COMPONENT(TextWrapAfter)
// PTGN_REGISTER_COMPONENT(TextColor)
// PTGN_REGISTER_COMPONENT(TextOutline)
// PTGN_REGISTER_COMPONENT(TextShadingColor)
// PTGN_REGISTER_COMPONENT(TextureFormat)
// PTGN_REGISTER_COMPONENT(TextureWrapping)
// PTGN_REGISTER_COMPONENT(TextureScaling)
// PTGN_REGISTER_COMPONENT(TextureHandle)
// PTGN_REGISTER_COMPONENT(CameraInstance)
// PTGN_REGISTER_COMPONENT(SceneTransition)
// PTGN_REGISTER_COMPONENT(TargetFollowConfig)
// PTGN_REGISTER_COMPONENT(PathFollowConfig)
// PTGN_REGISTER_COMPONENT(ShakeConfig)
// PTGN_REGISTER_COMPONENT(TranslateEffect)
// PTGN_REGISTER_COMPONENT(RotateEffect)
// PTGN_REGISTER_COMPONENT(ScaleEffect)
// PTGN_REGISTER_COMPONENT(TintEffect)
// PTGN_REGISTER_COMPONENT(FollowEffect)
// PTGN_REGISTER_COMPONENT(BounceEffect)
// PTGN_REGISTER_COMPONENT(ShakeEffect)
// PTGN_REGISTER_COMPONENT(TweenData)
//  PTGN_REGISTER_COMPONENT(ButtonState)
//  PTGN_REGISTER_COMPONENT(InternalButtonState)
//  PTGN_REGISTER_COMPONENT(ButtonToggled)
//  PTGN_REGISTER_COMPONENT(ButtonDisabledTexture)
//  PTGN_REGISTER_COMPONENT(ButtonTextFixedSize)
//  PTGN_REGISTER_COMPONENT(ButtonBorderWidth)
//  PTGN_REGISTER_COMPONENT(ButtonBackgroundWidth)
//  PTGN_REGISTER_COMPONENT(ButtonColor)
//  PTGN_REGISTER_COMPONENT(ButtonColorToggled)
//  PTGN_REGISTER_COMPONENT(ButtonTint)
//  PTGN_REGISTER_COMPONENT(ButtonTintToggled)
//  PTGN_REGISTER_COMPONENT(ButtonBorderColor)
//  PTGN_REGISTER_COMPONENT(ButtonBorderColorToggled)
//  PTGN_REGISTER_COMPONENT(ButtonTexture)
//  PTGN_REGISTER_COMPONENT(ButtonTextureToggled)
//  PTGN_REGISTER_COMPONENT(ButtonTextToggled)
PTGN_REGISTER_COMPONENT(Rect)
PTGN_REGISTER_COMPONENT(Circle)
PTGN_REGISTER_COMPONENT(Line)
PTGN_REGISTER_COMPONENT(Polygon)
PTGN_REGISTER_COMPONENT(Capsule)
PTGN_REGISTER_COMPONENT(Triangle)
// PTGN_REGISTER_COMPONENT(Camera)
// PTGN_REGISTER_COMPONENT(Button)
// PTGN_REGISTER_COMPONENT(Entity)
// PTGN_REGISTER_COMPONENT(RenderTarget)
// PTGN_REGISTER_COMPONENT(Text)
// PTGN_REGISTER_COMPONENT(Sprite)
// PTGN_REGISTER_COMPONENT(Animation)
// PTGN_REGISTER_COMPONENT(PhysicsBody)
// PTGN_REGISTER_COMPONENT(PointLight)
// PTGN_REGISTER_COMPONENT(ParticleEmitter)
// PTGN_REGISTER_COMPONENT(ToggleButton)
