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

namespace ptgn {

// Fundamental entity state.
PTGN_REGISTER_COMPONENT(Transform, { .group = "Core" });
PTGN_REGISTER_COMPONENT(impl::Scripts, { .group = "Core" });

// Drawable description and geometry.
PTGN_REGISTER_COMPONENT(StyledText, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(TextBox, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(impl::GraphicsData, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(impl::ParticleEmitterComponent, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(LightConfig, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(impl::ShadowCaster, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(impl::IDrawable, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Depth, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Visible, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Tint, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(FillStyle, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(BlendMode, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Color, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Origin, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Rect, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Circle, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(RoundedRect, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Polygon, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Ellipse, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Triangle, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Line, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Capsule, { .group = "Drawing" });
PTGN_REGISTER_COMPONENT(Arc, { .group = "Drawing" });

// Input and interaction.
PTGN_REGISTER_COMPONENT(impl::Interactive, { .group = "Interactive" });
PTGN_REGISTER_COMPONENT(impl::Draggable, { .group = "Interactive" });
PTGN_REGISTER_COMPONENT(impl::Dropzone, { .group = "Interactive" });
PTGN_REGISTER_COMPONENT(InteractionLock, { .group = "Interactive" });
PTGN_REGISTER_COMPONENT(impl::InteractiveTag, { .group = "Interactive" });

// Physics and movement.
PTGN_REGISTER_COMPONENT(Collider, { .group = "Physics" });
PTGN_REGISTER_COMPONENT(RigidBody, { .group = "Physics" });
PTGN_REGISTER_COMPONENT(BoundaryBehavior, { .group = "Physics" });
PTGN_REGISTER_COMPONENT(Lifetime, { .group = "Physics" });
PTGN_REGISTER_COMPONENT(TopDownMovement, { .group = "Physics" });
PTGN_REGISTER_COMPONENT(PlatformerMovement, { .group = "Physics" });
PTGN_REGISTER_COMPONENT(PlatformerJump, { .group = "Physics" });

// Asset references.
PTGN_REGISTER_COMPONENT(TextureKey, { .group = "Asset" });
PTGN_REGISTER_COMPONENT(FontKey, { .group = "Asset" });
PTGN_REGISTER_COMPONENT(AudioKey, { .group = "Asset" });
PTGN_REGISTER_COMPONENT(ShaderKey, { .group = "Asset" });
PTGN_REGISTER_COMPONENT(JsonKey, { .group = "Asset" });

// Button behavior, visuals, animation, and audio.
PTGN_REGISTER_COMPONENT(impl::ButtonData, { .group = "Button" });
PTGN_REGISTER_COMPONENT(
	impl::ButtonAnimationPart,
	{ .group = "Button" }
);
PTGN_REGISTER_COMPONENT(ButtonBackgroundVisuals, { .group = "Button" });
PTGN_REGISTER_COMPONENT(ButtonBorderVisuals, { .group = "Button" });
PTGN_REGISTER_COMPONENT(ButtonSpriteVisuals, { .group = "Button" });
PTGN_REGISTER_COMPONENT(ButtonTextVisuals, { .group = "Button" });
PTGN_REGISTER_COMPONENT(ButtonSounds, { .group = "Button" });

// Parent inheritance.
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentOffset,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentImmovable,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentTransform,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentPosition,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentRotation,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentScale,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentDepth,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentVisibility,
	{ .group = "Hierarchy" }
);
PTGN_REGISTER_COMPONENT(
	impl::IgnoreParentTint,
	{ .group = "Hierarchy" }
);

// Post-processing effects.
PTGN_REGISTER_COMPONENT(impl::EffectTag, { .group = "Effect" });
PTGN_REGISTER_COMPONENT(impl::HDREffectTag, { .group = "Effect" });
PTGN_REGISTER_COMPONENT(EffectMargin, { .group = "Effect" });
PTGN_REGISTER_COMPONENT(Bloom, { .group = "Effect" });
PTGN_REGISTER_COMPONENT(Blur, { .group = "Effect" });
PTGN_REGISTER_COMPONENT(GaussianBlur, { .group = "Effect" });

// Cameras and render targets.
PTGN_REGISTER_COMPONENT(
	impl::RenderTargetSize,
	{ .group = "Rendering" }
);
PTGN_REGISTER_COMPONENT(impl::RenderMask, { .group = "Rendering" });
PTGN_REGISTER_COMPONENT(impl::CameraMask, { .group = "Rendering" });
PTGN_REGISTER_COMPONENT(impl::CameraData, { .group = "Rendering" });
PTGN_REGISTER_COMPONENT(impl::ClearColor, { .group = "Rendering" });
PTGN_REGISTER_COMPONENT(impl::ClearDepth, { .group = "Rendering" });
PTGN_REGISTER_COMPONENT(impl::ClearStencil, { .group = "Rendering" });
PTGN_REGISTER_COMPONENT(impl::UILayer, { .group = "Rendering" });

// Internal graphics state used by drawable entities.
PTGN_REGISTER_COMPONENT(impl::TextureSize, { .group = "Animation" });
PTGN_REGISTER_COMPONENT(impl::TextureCrop, { .group = "Animation" });
PTGN_REGISTER_COMPONENT(impl::AnimationData, { .group = "Animation" });
PTGN_REGISTER_COMPONENT(impl::Offsets, { .group = "Animation" });

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
