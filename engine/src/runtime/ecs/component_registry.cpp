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
#include "runtime/graphics/custom_shader.h"
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
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"

namespace ptgn {

PTGN_REGISTER_COMPONENT(Transform);
PTGN_REGISTER_COMPONENT(impl::Scripts);
PTGN_REGISTER_COMPONENT(impl::ParentRenderTarget);

PTGN_REGISTER_COMPONENT(impl::TextData);
PTGN_REGISTER_COMPONENT(impl::GraphicsData);
PTGN_REGISTER_COMPONENT(impl::ParticleEmitterData);
PTGN_REGISTER_COMPONENT(LightData);
PTGN_REGISTER_COMPONENT(impl::ShadowCaster);
PTGN_REGISTER_COMPONENT(impl::IDrawable);
PTGN_REGISTER_COMPONENT(Depth);
PTGN_REGISTER_COMPONENT(Visible);
PTGN_REGISTER_COMPONENT(Tint);
PTGN_REGISTER_COMPONENT(FillStyle);
PTGN_REGISTER_COMPONENT(BlendMode);
PTGN_REGISTER_COMPONENT(Color);
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

PTGN_REGISTER_COMPONENT(impl::Interactive);
PTGN_REGISTER_COMPONENT(impl::Draggable);
PTGN_REGISTER_COMPONENT(impl::Dropzone);
PTGN_REGISTER_COMPONENT(InteractionLock);
PTGN_REGISTER_COMPONENT(impl::InteractiveTag);

PTGN_REGISTER_COMPONENT(Collider);
PTGN_REGISTER_COMPONENT(RigidBody);
PTGN_REGISTER_COMPONENT(BoundaryBehavior);
PTGN_REGISTER_COMPONENT(Lifetime);
PTGN_REGISTER_COMPONENT(TopDownMovement);
PTGN_REGISTER_COMPONENT(PlatformerMovement);
PTGN_REGISTER_COMPONENT(PlatformerJump);

PTGN_REGISTER_COMPONENT(TextureKey);
PTGN_REGISTER_COMPONENT(FontKey);
PTGN_REGISTER_COMPONENT(AudioKey);
PTGN_REGISTER_COMPONENT(ShaderKey);
PTGN_REGISTER_COMPONENT(JsonKey);

PTGN_REGISTER_COMPONENT(impl::ButtonData);
PTGN_REGISTER_COMPONENT(impl::ButtonAnimationPart);
PTGN_REGISTER_COMPONENT(ButtonBackgroundVisuals);
PTGN_REGISTER_COMPONENT(ButtonBorderVisuals);
PTGN_REGISTER_COMPONENT(ButtonSpriteVisuals);
PTGN_REGISTER_COMPONENT(ButtonTextVisuals);
PTGN_REGISTER_COMPONENT(ButtonSounds);
PTGN_REGISTER_COMPONENT(impl::ToggleButtonData);
PTGN_REGISTER_COMPONENT(impl::ToggleButtonGroupData);
PTGN_REGISTER_COMPONENT(impl::ToggleButtonGroupItem);
PTGN_REGISTER_COMPONENT(impl::DropdownData);
PTGN_REGISTER_COMPONENT(impl::DropdownItem);
PTGN_REGISTER_COMPONENT(impl::TooltipData);
PTGN_REGISTER_COMPONENT(impl::TooltipHoverData);
PTGN_REGISTER_COMPONENT(impl::TooltipBackgroundPart);
PTGN_REGISTER_COMPONENT(impl::TooltipTextPart);

PTGN_REGISTER_COMPONENT(impl::IgnoreParentOffset);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentImmovable);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentTransform);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentPosition);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentRotation);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentScale);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentDepth);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentVisibility);
PTGN_REGISTER_COMPONENT(impl::IgnoreParentTint);

PTGN_REGISTER_COMPONENT(impl::EffectTag);
PTGN_REGISTER_COMPONENT(impl::HDREffectTag);
PTGN_REGISTER_COMPONENT(EffectMargin);
PTGN_REGISTER_COMPONENT(Bloom);
PTGN_REGISTER_COMPONENT(Blur);
PTGN_REGISTER_COMPONENT(GaussianBlur);

PTGN_REGISTER_COMPONENT(impl::RenderTargetSize);
PTGN_REGISTER_COMPONENT(impl::RenderMask);
PTGN_REGISTER_COMPONENT(impl::CameraMask);
PTGN_REGISTER_COMPONENT(impl::CameraData);
PTGN_REGISTER_COMPONENT(impl::ClearColor);
PTGN_REGISTER_COMPONENT(impl::ClearDepth);
PTGN_REGISTER_COMPONENT(impl::ClearStencil);
PTGN_REGISTER_COMPONENT(impl::UILayer);
PTGN_REGISTER_COMPONENT(Material);

PTGN_REGISTER_COMPONENT(impl::TextureSize);
PTGN_REGISTER_COMPONENT(impl::TextureCrop);
PTGN_REGISTER_COMPONENT(impl::AnimationData);
PTGN_REGISTER_COMPONENT(impl::Offsets);

namespace impl {

void EnsureEngineComponentsRegistered() {
	// Referencing this function forces the linker to include this object file.
}

} // namespace impl

} // namespace ptgn
