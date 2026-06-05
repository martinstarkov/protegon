#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class DrawContext;
class RenderTarget;

inline constexpr TextureFormat kDefaultRenderTargetFormat{ kDefaultHDRFormat };
inline constexpr Color kDefaultRenderTargetClearColor{ color::Transparent };

RenderTarget CreateRenderTarget(Scene&, V2_int, Color, TextureFormat);
RenderTarget CreateRenderTarget(Scene&, ResizeType, Color, TextureFormat);

namespace impl {

struct ClearColor {
	Color color{ color::Transparent };
};

struct ClearDepth {
	Depth depth{ 1.0f };
};

struct ClearStencil {
	Stencil stencil{ 0 };
};

class RenderTargetGameResizeScript : public Script {
public:
	void OnEvent(Event event) override;
};

class RenderTargetDisplayResizeScript : public Script {
public:
	void OnEvent(Event event) override;
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

	/// @return The scale of the render target size relative to the game size.
	V2_float GetScale() const;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;
	TextureParams GetParams() const;
	TextureDesc GetDesc() const;

private:
	friend class Scene;

	friend RenderTarget CreateRenderTarget(Scene&, V2_int, Color, TextureFormat);
	friend RenderTarget CreateRenderTarget(Scene&, ResizeType, Color, TextureFormat);

	static void AddRenderTargetComponents(
		RenderTarget render_target, Scene& scene, V2_int size, Color clear_color,
		TextureFormat format
	);

	static void AddRenderTargetComponents(
		RenderTarget render_target, Scene& scene, ResizeType resize_to_resolution,
		Color clear_color, TextureFormat texture_format
	);

	impl::TextureId GetTexture() const;
};

namespace impl {

struct ParentRenderTarget {
	RenderTarget render_target;
};

} // namespace impl

/// Create a render target with a custom size.
/// @param size The size of the render target and its camera viewport.
/// @param clear_color The color to which the render target is cleared.
/// @param Texture format of the render target texture. Ensure this complies with possible HDR
/// requirements.
RenderTarget CreateRenderTarget(
	Scene& scene, V2_int size, Color clear_color = kDefaultRenderTargetClearColor,
	TextureFormat texture_format = kDefaultRenderTargetFormat
);

/// Create a render target that is continuously sized to the specified resolution.
/// @param resize_to_resolution Which resolution the render target automatically resizes to.
/// @param clear_color The color to which the render target is cleared.
/// @param Texture format of the render target texture. Ensure this complies with possible HDR
/// requirements.
RenderTarget CreateRenderTarget(
	Scene& scene, ResizeType resize_to_resolution,
	Color clear_color			 = kDefaultRenderTargetClearColor,
	TextureFormat texture_format = kDefaultRenderTargetFormat
);

PTGN_REGISTER_DRAWABLE(RenderTarget);

} // namespace ptgn