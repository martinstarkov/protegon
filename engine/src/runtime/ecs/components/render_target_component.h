#pragma once

#include <vector>

#include "core/event/dispatcher.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class Renderer;

enum class ResizeMode {
	GameSize,
	DisplaySize
};

namespace impl {

class RenderTargetGameResizeScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;
};

class RenderTargetDisplayResizeScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;
};

struct DisplayList {
	std::vector<Entity> entities;
};

// TODO: Fix.
// struct RenderTarget {
//	static void Draw(Renderer& renderer, Entity entity);
//};

// TODO: Add clear color to render target as an optional component. Otherwise they should be cleared
// to transparent.

Entity CreateRenderTarget(
	Entity render_target, Renderer& renderer, ResizeMode resize_to_resolution,
	TextureFormat texture_format
);

} // namespace impl

/// Create a render target with a custom size.
/// @param size The size of the render target and its camera viewport.
/// @param clear_color The background color of the render target.
/// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
Entity CreateRenderTarget(
	Scene& scene, Renderer& renderer, V2_int size,
	TextureFormat texture_format = TextureFormat::RGBA8
);

/// Create a render target that is continuously sized to the specified resolution.
/// @param resize_to_resolution Which resolution the render target automatically resizes to.
/// @param clear_color The background color of the render target.
/// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
Entity CreateRenderTarget(
	Scene& scene, Renderer& renderer, ResizeMode resize_to_resolution = ResizeMode::DisplaySize,
	TextureFormat texture_format = TextureFormat::RGBA8
);

// TODO: Fix.
// PTGN_REGISTER_DRAWABLE(impl::RenderTarget);

} // namespace ptgn