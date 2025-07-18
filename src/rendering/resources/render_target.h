#pragma once

#include <unordered_set>

#include "components/drawable.h"
#include "components/generic.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/resources/texture.h"

namespace ptgn {

class Scene;
class Camera;
class RenderTarget;

namespace impl {

RenderTarget CreateRenderTarget(
	const Entity& entity, const V2_float& size, const Color& clear_color,
	TextureFormat texture_format
);

class RenderData;

struct RenderTargetEntities {
	std::unordered_set<Entity> entities;
};

struct ClearColor : public ColorComponent {
	using ColorComponent::ColorComponent;

	ClearColor() : ColorComponent{ color::Transparent } {}
};

} // namespace impl

// Each render target is initialized with a window camera.
class RenderTarget : public Entity, public Drawable<RenderTarget> {
public:
	// A default render target will result in the screen being used as the render target.
	RenderTarget()									 = default;
	RenderTarget(const RenderTarget&)				 = default;
	RenderTarget& operator=(const RenderTarget&)	 = default;
	RenderTarget(RenderTarget&&) noexcept			 = default;
	RenderTarget& operator=(RenderTarget&&) noexcept = default;
	~RenderTarget()									 = default;

	RenderTarget(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// TODO: Implement window resizing.
	// Create a render target that is continuously sized to the window.
	// @param clear_color The background color of the render target.
	// explicit RenderTarget(const Color& clear_color);

	void ClearEntities();
	void AddEntity(Entity& entity);

	// @return The clear color of the render target.
	[[nodiscard]] Color GetClearColor() const;

	// @param clear_color The clear color to set for the render target. This only takes effect after
	// the render target is cleared.
	void SetClearColor(const Color& clear_color);

	// @return Texture attached to the render target.
	[[nodiscard]] const impl::Texture& GetTexture() const;
	[[nodiscard]] impl::Texture& GetTexture();

	// @return Frame buffer of the render target.
	[[nodiscard]] const impl::FrameBuffer& GetFrameBuffer() const;

	void Bind() const;

	// Clear the render target. This function will bind the render target's frame buffer.
	void Clear() const;

	// Clear the render target to a specified color without modifying its internally stored clear
	// color. This function will bind the render target's frame buffer.
	void ClearToColor(const Color& color) const;

private:
	friend class impl::RenderData;

	friend RenderTarget impl::CreateRenderTarget(
		const Entity& entity, const V2_float& size, const Color& clear_color,
		TextureFormat texture_format
	);

	// Draw an entity to the render target.
	// The entity must have the Transform and Visible components.
	void Draw(const Entity& entity) const;

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param shader Specify a custom shader when drawing the render target. If {},
	// uses the default screen shader.
	// @param clear_after_draw If true, clears the render target after drawing it.
	/*void Draw(
		const TextureInfo& texture_info = {}, const Shader& shader = {},
		bool clear_after_draw = true
	) const;*/

	// TODO: Add window subscribe stuff here.
	// Subscribes viewport to being resized to window size.
	// Will also set the viewport to the current window size.
	// void SubscribeToEvents();

	// TODO: Add window subscribe stuff here.
	// Unsubscribes viewport from being resized to window size.
	// void UnsubscribeFromEvents() const;

	//// Only to be used by the renderer screen target. Draws screen target to the screen frame
	/// buffer / (id=0) with the default screen shader.
	// void DrawToScreen() const;

	//// @param shader {} will result in default screen shader being used.
	// void Draw(const TextureInfo& texture_info, Shader shader, bool clear_after_draw) const;
};

// Create a render target with a custom size.
// @param size The size of the render target.
// @param clear_color The background color of the render target.
RenderTarget CreateRenderTarget(
	Scene& scene, const V2_float& size, const Color& clear_color = color::Transparent,
	TextureFormat texture_format = TextureFormat::RGBA8888
);

} // namespace ptgn