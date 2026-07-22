#include "runtime/ecs/component_registry.h"

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/tween.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

namespace ptgn {

PTGN_REGISTER_COMPONENT(Transform);
PTGN_REGISTER_COMPONENT(Depth);
PTGN_REGISTER_COMPONENT(Tint);
PTGN_REGISTER_COMPONENT(impl::IDrawable);
PTGN_REGISTER_COMPONENT(Color);
PTGN_REGISTER_COMPONENT(Visible);
PTGN_REGISTER_COMPONENT(Origin);
PTGN_REGISTER_COMPONENT(Rect);
PTGN_REGISTER_COMPONENT(Circle);
PTGN_REGISTER_COMPONENT(RoundedRect);
PTGN_REGISTER_COMPONENT(Polygon);
PTGN_REGISTER_COMPONENT(Ellipse);
PTGN_REGISTER_COMPONENT(Triangle);
PTGN_REGISTER_COMPONENT(Line);
PTGN_REGISTER_COMPONENT(Capsule);
PTGN_REGISTER_COMPONENT(Arc);
PTGN_REGISTER_COMPONENT(FillStyle);
PTGN_REGISTER_COMPONENT(BlendMode);

PTGN_REGISTER_COMPONENT(impl::Interactive, { .group = "Interactive Components" });
PTGN_REGISTER_COMPONENT(impl::Draggable, { .group = "Interactive Components" });
PTGN_REGISTER_COMPONENT(impl::Dropzone, { .group = "Interactive Components" });
PTGN_REGISTER_COMPONENT(InteractionLock, { .group = "Interactive Components" });
PTGN_REGISTER_COMPONENT(impl::InteractiveTag, { .group = "Interactive Components" });

PTGN_REGISTER_COMPONENT(StyledText);
PTGN_REGISTER_COMPONENT(TextBox);

PTGN_REGISTER_COMPONENT(Collider, { .group = "Physics Components" });
PTGN_REGISTER_COMPONENT(RigidBody, { .group = "Physics Components" });
PTGN_REGISTER_COMPONENT(BoundaryBehavior, { .group = "Physics Components" });
PTGN_REGISTER_COMPONENT(Lifetime, { .group = "Physics Components" });
PTGN_REGISTER_COMPONENT(TopDownMovement, { .group = "Physics Components" });
PTGN_REGISTER_COMPONENT(PlatformerMovement, { .group = "Physics Components" });
PTGN_REGISTER_COMPONENT(PlatformerJump, { .group = "Physics Components" });

PTGN_REGISTER_COMPONENT(impl::ParticleEmitterComponent);
PTGN_REGISTER_COMPONENT(TextureKey);
PTGN_REGISTER_COMPONENT(FontKey);
PTGN_REGISTER_COMPONENT(AudioKey);
PTGN_REGISTER_COMPONENT(ShaderKey);
PTGN_REGISTER_COMPONENT(JsonKey);
PTGN_REGISTER_COMPONENT(LightConfig);
PTGN_REGISTER_COMPONENT(impl::ShadowCaster);

PTGN_REGISTER_COMPONENT(impl::ButtonData);
PTGN_REGISTER_COMPONENT(impl::ButtonAnimationPart);
PTGN_REGISTER_COMPONENT(impl::AnimationData);
PTGN_REGISTER_COMPONENT(impl::Offsets);
PTGN_REGISTER_COMPONENT(impl::TweenData);
PTGN_REGISTER_COMPONENT(ButtonBackgroundVisuals);
PTGN_REGISTER_COMPONENT(ButtonBorderVisuals);
PTGN_REGISTER_COMPONENT(ButtonSpriteVisuals);
PTGN_REGISTER_COMPONENT(ButtonTextVisuals);
PTGN_REGISTER_COMPONENT(ButtonSounds);

PTGN_REGISTER_COMPONENT(impl::IgnoreParentOffset, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentImmovable, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentTransform, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentPosition, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentRotation, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentScale, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentDepth, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentVisibility, { .group = "Ignore Components" });
PTGN_REGISTER_COMPONENT(impl::IgnoreParentTint, { .group = "Ignore Components" });

PTGN_REGISTER_COMPONENT(impl::EffectTag, { .group = "Effect Components" });
PTGN_REGISTER_COMPONENT(impl::HDREffectTag, { .group = "Effect Components" });
PTGN_REGISTER_COMPONENT(EffectMargin, { .group = "Effect Components" });
PTGN_REGISTER_COMPONENT(Bloom, { .group = "Effect Components" });
PTGN_REGISTER_COMPONENT(Blur, { .group = "Effect Components" });
PTGN_REGISTER_COMPONENT(GaussianBlur, { .group = "Effect Components" });

PTGN_REGISTER_COMPONENT(impl::RenderMask);
PTGN_REGISTER_COMPONENT(impl::CameraMask);
PTGN_REGISTER_COMPONENT(impl::CameraData);
PTGN_REGISTER_COMPONENT(impl::ClearColor);
PTGN_REGISTER_COMPONENT(impl::ClearDepth);
PTGN_REGISTER_COMPONENT(impl::ClearStencil);

PTGN_REGISTER_COMPONENT(impl::UILayer);
PTGN_REGISTER_COMPONENT(impl::TextureSize);
PTGN_REGISTER_COMPONENT(impl::TextureCrop);
PTGN_REGISTER_COMPONENT(impl::GraphicsData);

namespace impl {

void EnsureEngineComponentsRegistered() {
	// Intentionally empty.
	//
	// Referencing this function forces the linker to include this object file.
	// The PTGN_REGISTER_COMPONENT static initializers then register the
	// components before main().
}

} // namespace impl

} // namespace ptgn