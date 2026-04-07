#pragma once

#include <optional>

#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class DrawContext;
class SceneContext;
class RenderTarget;

/// @brief Determines which resolution the render target automatically resizes to when the game or
/// display is resized. GameSize resizes to the current game size, while DisplaySize resizes to the
/// current display size.
enum class ResizeMode {
	GameSize,
	DisplaySize
};

RenderTarget CreateRenderTarget(Scene&, V2_int, Color, TextureFormat);
RenderTarget CreateRenderTarget(Scene&, ResizeMode, Color, TextureFormat);

namespace impl {

class RenderTargetGameResizeScript : public Script {
public:
	void OnEvent(EventDispatcher dispatcher) override;
};

class RenderTargetDisplayResizeScript : public Script {
public:
	void OnEvent(EventDispatcher dispatcher) override;
};

} // namespace impl

class RenderTarget : public Entity {
public:
	RenderTarget() = default;
	explicit RenderTarget(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

	/// @brief Binds the render target's internal frame buffer as the current render target.
	void Bind();

	/// @brief Clears the render target internal frame buffer color attachment.
	/// Render target must be bound before calling this function.
	/// @param color If {}, uses the render target's clear color (default to color::Transparent).
	/// @param set_viewport If true, also sets the renderer viewport to match the render target
	/// size (previous viewport will be restored after the clear).
	void Clear(std::optional<Color> color = {}, bool set_viewport = true);

	void SetClearColor(Color clear_color);
	Color GetClearColor() const;

	/// @return The scale of the render target size relative to the game size.
	V2_float GetScale() const;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

private:
	friend class Scene;

	operator impl::RenderTargetId() const; // NOSONAR

	friend RenderTarget CreateRenderTarget(Scene&, V2_int, Color, TextureFormat);
	friend RenderTarget CreateRenderTarget(Scene&, ResizeMode, Color, TextureFormat);

	static void AddRenderTargetComponents(
		RenderTarget render_target, SceneContext& ctx, V2_int size, Color clear_color,
		TextureFormat format
	);

	static void AddRenderTargetComponents(
		RenderTarget render_target, SceneContext& ctx, ResizeMode resize_to_resolution,
		Color clear_color, TextureFormat texture_format
	);
};

namespace impl {

struct ParentRenderTarget {
	RenderTarget render_target;
};

} // namespace impl

/// Create a render target with a custom size.
/// @param size The size of the render target and its camera viewport.
/// @param clear_color The background color of the render target.
/// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
RenderTarget CreateRenderTarget(
	Scene& scene, V2_int size, Color clear_color = color::Transparent,
	TextureFormat texture_format = TextureFormat::RGBA8
);

/// Create a render target that is continuously sized to the specified resolution.
/// @param resize_to_resolution Which resolution the render target automatically resizes to.
/// @param clear_color The background color of the render target.
/// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
RenderTarget CreateRenderTarget(
	Scene& scene, ResizeMode resize_to_resolution = ResizeMode::DisplaySize,
	Color clear_color = color::Transparent, TextureFormat texture_format = TextureFormat::RGBA8
);

PTGN_REGISTER_DRAWABLE(RenderTarget);

} // namespace ptgn