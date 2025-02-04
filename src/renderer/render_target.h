#pragma once

#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/texture.h"

namespace ptgn {

// Each render target is initialized with a window camera.
class RenderTarget {
public:
	// A default render target will result in the screen being used as the render target.
	RenderTarget() = default;

	RenderTarget(const RenderTarget&)			 = delete;
	RenderTarget& operator=(const RenderTarget&) = delete;
	RenderTarget(RenderTarget&& other) noexcept;
	RenderTarget& operator=(RenderTarget&& other) noexcept;

	~RenderTarget();

	// Create a render target that is continuously sized to the window.
	// @param clear_color The background color of the render target.
	/*explicit RenderTarget(
		const Color& clear_color
	);*/

	// Create a render target with a custom size.
	// @param size The size of the render target.
	// @param clear_color The background color of the render target.
	RenderTarget(const V2_float& size, const Color& clear_color = color::Transparent);

	// Draw an entity to the render target.
	// The entity must have the Transform and Visible components.
	void Draw(ecs::Entity entity) const;

	// @return The clear color of the render target.
	[[nodiscard]] Color GetClearColor() const;

	// @param clear_color The clear color to set for the render target. This only takes effect after
	// the render target is cleared.
	void SetClearColor(const Color& clear_color);

	// @return Texture attached to the render target.
	[[nodiscard]] const impl::Texture& GetTexture() const;

	// @return Frame buffer of the render target.
	[[nodiscard]] const impl::FrameBuffer& GetFrameBuffer() const;

	void Bind() const;

	// Clear the render target.
	void Clear() const;

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param shader Specify a custom shader when drawing the render target. If {},
	// uses the default screen shader.
	// @param clear_after_draw If true, clears the render target after drawing it.
	/*void Draw(
		const TextureInfo& texture_info = {}, const Shader& shader = {},
		bool clear_after_draw = true
	) const;*/

private:
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

	impl::FrameBuffer frame_buffer_;
	Color clear_color_{ color::Transparent };
};

} // namespace ptgn