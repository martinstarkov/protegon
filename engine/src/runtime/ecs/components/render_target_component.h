#pragma once

#include "core/event/dispatcher.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class Renderer;
class RenderTarget;

enum class ResizeMode {
	GameSize,
	DisplaySize
};

RenderTarget CreateRenderTarget(Scene&, V2_int, TextureFormat);
RenderTarget CreateRenderTarget(Scene&, ResizeMode, TextureFormat);

namespace impl {

class RenderTargetGameResizeScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;
};

class RenderTargetDisplayResizeScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;
};

struct ParentRenderTarget {
	std::size_t render_target{ 0 };
};

// TODO: Add clear color to render target as an optional component. Otherwise they should be cleared
// to transparent.

} // namespace impl

class RenderTarget : public Entity {
public:
	RenderTarget() = default;
	explicit RenderTarget(Entity entity);

	static void Draw(Renderer& renderer, Entity entity);

	void Bind();
	void Clear(Color color = color::Transparent);

	// TODO: Add.
	// void SetClearColor(Color clear_color);
	//[[nodiscard]] Color GetClearColor() const;

	[[nodiscard]] V2_int GetSize() const;
	[[nodiscard]] TextureFormat GetFormat() const;

	operator impl::TextureId() const;

private:
	friend RenderTarget CreateRenderTarget(Scene&, V2_int, TextureFormat);
	friend RenderTarget CreateRenderTarget(Scene&, ResizeMode, TextureFormat);

	static void AddRenderTargetComponents(
		RenderTarget render_target, Renderer& renderer, V2_int size, TextureFormat format
	);

	static void AddRenderTargetComponents(
		RenderTarget render_target, Renderer& renderer, ResizeMode resize_to_resolution,
		TextureFormat texture_format
	);
};

/// Create a render target with a custom size.
/// @param size The size of the render target and its camera viewport.
/// @param clear_color The background color of the render target.
/// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
RenderTarget CreateRenderTarget(
	Scene& scene, V2_int size, TextureFormat texture_format = TextureFormat::RGBA8
);

/// Create a render target that is continuously sized to the specified resolution.
/// @param resize_to_resolution Which resolution the render target automatically resizes to.
/// @param clear_color The background color of the render target.
/// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
RenderTarget CreateRenderTarget(
	Scene& scene, ResizeMode resize_to_resolution = ResizeMode::DisplaySize,
	TextureFormat texture_format = TextureFormat::RGBA8
);

PTGN_REGISTER_DRAWABLE(RenderTarget);

} // namespace ptgn