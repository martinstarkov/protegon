#pragma once

#include <optional>

#include "core/graphics/color.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class DrawContext;
class RenderTarget;

inline constexpr TextureFormat kDefaultRenderTargetFormat{ kDefaultHDRFormat };
inline constexpr Color kDefaultRenderTargetClearColor{ color::Transparent };

namespace impl {

struct RenderTargetSize {
	bool follow_display_size{ true };
	/// @brief Must be above zero.
	V2_int size{ 1, 1 };

	PTGN_REFLECT(RenderTargetSize, follow_display_size, size)
};

struct ClearColor {
	Color color{ color::Transparent };

	PTGN_REFLECT_VALUE(ClearColor, color)
};

struct ClearDepth {
	Depth depth{ 1.0f };

	PTGN_REFLECT_VALUE(ClearDepth, depth)
};

struct ClearStencil {
	Stencil stencil{ 0 };

	PTGN_REFLECT_VALUE(ClearStencil, stencil)
};

} // namespace impl

class RenderTarget : public Entity {
public:
	RenderTarget() = default;
	explicit RenderTarget(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	/// @brief Binds the render target's internal frame buffer as the current render target.
	void Bind();

	/// @brief Clears the render target's color attachment.
	/// @param color Clear color. If empty, uses the target's configured clear color.
	/// @param restore_bind If true, restores the previously bound render target after clearing.
	void ClearColor(std::optional<Color> color = {}, bool restore_bind = true);

	/// @brief Clears the render target's depth attachment.
	/// @param depth Clear depth. If empty, uses the default depth clear value.
	/// @param restore_bind If true, restores the previously bound render target after clearing.
	void ClearDepth(std::optional<Depth> depth = {}, bool restore_bind = true);

	/// @brief Clears the render target's stencil attachment.
	/// @param stencil Clear stencil. If empty, uses the default stencil clear value.
	/// @param restore_bind If true, restores the previously bound render target after clearing.
	void ClearStencil(std::optional<Stencil> stencil = {}, bool restore_bind = true);

	/// @brief Clears the render target's depth and stencil attachment.
	/// @param depth_stencil Clear depth/stencil values. If empty, uses default clear values.
	/// @param restore_bind If true, restores the previously bound render target after clearing.
	void ClearDepthStencil(
		std::optional<DepthStencil> depth_stencil = {}, bool restore_bind = true
	);

	void SetClearColor(Color clear_color);
	std::optional<Color> GetClearColor() const;

	void SetClearDepth(Depth clear_depth);
	std::optional<Depth> GetClearDepth() const;

	void SetClearStencil(Stencil clear_stencil);
	std::optional<Stencil> GetClearStencil() const;

	void SetClearDepthStencil(DepthStencil clear_depth_stencil);
	std::optional<DepthStencil> GetClearDepthStencil() const;

	/// @brief Enables or disables automatic display size tracking.
	/// When disabled, the current framebuffer size becomes the custom size.
	RenderTarget& SetFollowDisplaySize(bool follow_display_size);

	/// @brief Sets a custom render target size and disables display size tracking.
	RenderTarget& SetSize(V2_int size);

	/// @return Whether the target tracks the renderer display size.
	bool FollowsDisplaySize() const;

	/// @return The desired size stored by RenderTargetSize.
	/// When following the display, this is refreshed every frame.
	V2_int GetConfiguredSize() const;

	/// @return The scale of the actual framebuffer size relative to the logical size.
	V2_float GetScale() const;

	/// @return The actual framebuffer size.
	V2_int GetSize() const;

	TextureFormat GetFormat() const;
	TextureParams GetParams() const;
	TextureDesc GetDesc() const;

	/// @brief Synchronizes the desired component size and actual frame buffer.
	/// @param display_size Current renderer display size.
	/// @return Whether the frame buffer was resized.
	bool UpdateSize(V2_int display_size);

private:
	friend class Scene;

	impl::TextureId GetTexture() const;
};

/// @brief Create a render target with a custom size.
/// A zero size is retained as backwards-compatible shorthand for following the display size.
/// @param size The size of the render target.
/// @param clear_color The color to which the render target is cleared.
/// @param texture_format Format of the render target texture.
RenderTarget CreateRenderTarget(
	Scene& scene, Transform transform, V2_int size,
	Color clear_color			 = kDefaultRenderTargetClearColor,
	TextureFormat texture_format = kDefaultRenderTargetFormat
);

/// @brief Create a render target that continuously follows the renderer display size.
/// @param clear_color The color to which the render target is cleared.
/// @param texture_format Format of the render target texture.
RenderTarget CreateRenderTarget(
	Scene& scene, Transform transform = {}, Color clear_color = kDefaultRenderTargetClearColor,
	TextureFormat texture_format = kDefaultRenderTargetFormat
);

PTGN_REGISTER_DRAWABLE(RenderTarget, { .name = "Render Target" });

} // namespace ptgn
