#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>

#include "common/type_info.h"
#include "components/common.h"
#include "components/draw.h"
#include "components/input.h"
#include "components/lifetime.h"
#include "components/movement.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/timer.h"
#include "debug/log.h"
#include "math/hash.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/flip.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/graphics/vfx/particle.h"
#include "rendering/resources/font.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/text.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "scene/scene_transition.h"
#include "tweening/follow_config.h"
#include "tweening/shake_config.h"
#include "tweening/tween.h"
#include "tweening/tween_effects.h"
#include "ui/button.h"

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
		constexpr auto class_name{ type_name<T>() };
		// PTGN_LOG("Registering component: ", class_name);
		auto& registry{ GetData() };
		registry[Hash(class_name)] = [](Manager& manager) {
			manager.template RegisterType<T>();
		};
		return true;
	}

	static void AddTypes(Manager& manager) {
		const auto& registry{ GetData() };
		for (const auto& [type_name, component_func] : registry) {
			std::invoke(component_func, manager);
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

PTGN_REGISTER_COMPONENT(Enabled)
PTGN_REGISTER_COMPONENT(Visible)
PTGN_REGISTER_COMPONENT(Tint)
PTGN_REGISTER_COMPONENT(Depth)
PTGN_REGISTER_COMPONENT(LineWidth)
PTGN_REGISTER_COMPONENT(TextureCrop)
PTGN_REGISTER_COMPONENT(AnimationInfo)
PTGN_REGISTER_COMPONENT(IDrawable)
PTGN_REGISTER_COMPONENT(Interactive)
PTGN_REGISTER_COMPONENT(Draggable)
PTGN_REGISTER_COMPONENT(InteractiveCircles)
PTGN_REGISTER_COMPONENT(InteractiveRects)
PTGN_REGISTER_COMPONENT(Lifetime)
PTGN_REGISTER_COMPONENT(TopDownMovement)
PTGN_REGISTER_COMPONENT(PlatformerMovement)
PTGN_REGISTER_COMPONENT(PlatformerJump)
PTGN_REGISTER_COMPONENT(Offsets)
PTGN_REGISTER_COMPONENT(Transform)
PTGN_REGISTER_COMPONENT(UUID)
PTGN_REGISTER_COMPONENT(ChildKey)
PTGN_REGISTER_COMPONENT(Parent)
PTGN_REGISTER_COMPONENT(Children)
PTGN_REGISTER_COMPONENT(Timer)
PTGN_REGISTER_COMPONENT(BoxCollider)
PTGN_REGISTER_COMPONENT(CircleCollider)
PTGN_REGISTER_COMPONENT(RigidBody)
PTGN_REGISTER_COMPONENT(BlendMode)
PTGN_REGISTER_COMPONENT(Color)
PTGN_REGISTER_COMPONENT(Flip)
PTGN_REGISTER_COMPONENT(Origin)
PTGN_REGISTER_COMPONENT(LightProperties)
PTGN_REGISTER_COMPONENT(Particle)
PTGN_REGISTER_COMPONENT(ParticleInfo)
PTGN_REGISTER_COMPONENT(ParticleEmitterComponent)
PTGN_REGISTER_COMPONENT(FontRenderMode)
PTGN_REGISTER_COMPONENT(FontStyle)
PTGN_REGISTER_COMPONENT(FontKey)
PTGN_REGISTER_COMPONENT(ClearColor)
PTGN_REGISTER_COMPONENT(TextJustify)
PTGN_REGISTER_COMPONENT(TextContent)
PTGN_REGISTER_COMPONENT(FontSize)
PTGN_REGISTER_COMPONENT(TextLineSkip)
PTGN_REGISTER_COMPONENT(TextWrapAfter)
PTGN_REGISTER_COMPONENT(TextColor)
PTGN_REGISTER_COMPONENT(TextOutline)
PTGN_REGISTER_COMPONENT(TextShadingColor)
PTGN_REGISTER_COMPONENT(TextureFormat)
PTGN_REGISTER_COMPONENT(TextureWrapping)
PTGN_REGISTER_COMPONENT(TextureScaling)
PTGN_REGISTER_COMPONENT(TextureHandle)
PTGN_REGISTER_COMPONENT(CameraInfo)
PTGN_REGISTER_COMPONENT(SceneComponent)
PTGN_REGISTER_COMPONENT(TransitionType)
PTGN_REGISTER_COMPONENT(SceneTransition)
PTGN_REGISTER_COMPONENT(FollowConfig)
PTGN_REGISTER_COMPONENT(ShakeConfig)
PTGN_REGISTER_COMPONENT(FollowEffect)
PTGN_REGISTER_COMPONENT(TranslateEffect)
PTGN_REGISTER_COMPONENT(RotateEffect)
PTGN_REGISTER_COMPONENT(ScaleEffect)
PTGN_REGISTER_COMPONENT(TintEffect)
PTGN_REGISTER_COMPONENT(BounceEffect)
PTGN_REGISTER_COMPONENT(ShakeEffect)
PTGN_REGISTER_COMPONENT(TweenInstance)
PTGN_REGISTER_COMPONENT(ButtonState)
PTGN_REGISTER_COMPONENT(InternalButtonState)
PTGN_REGISTER_COMPONENT(ButtonToggled)
PTGN_REGISTER_COMPONENT(ButtonDisabledTextureKey)
PTGN_REGISTER_COMPONENT(ButtonTextFixedSize)
PTGN_REGISTER_COMPONENT(ButtonBorderWidth)
PTGN_REGISTER_COMPONENT(ButtonBackgroundWidth)
PTGN_REGISTER_COMPONENT(ButtonColor)
PTGN_REGISTER_COMPONENT(ButtonColorToggled)
PTGN_REGISTER_COMPONENT(ButtonTint)
PTGN_REGISTER_COMPONENT(ButtonTintToggled)
PTGN_REGISTER_COMPONENT(ButtonBorderColor)
PTGN_REGISTER_COMPONENT(ButtonBorderColorToggled)
PTGN_REGISTER_COMPONENT(ButtonTexture)
PTGN_REGISTER_COMPONENT(ButtonTextureToggled)
PTGN_REGISTER_COMPONENT(ButtonText)
PTGN_REGISTER_COMPONENT(ButtonTextToggled)
PTGN_REGISTER_COMPONENT(ButtonSize)
PTGN_REGISTER_COMPONENT(ButtonRadius)
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
