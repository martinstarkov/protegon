#pragma once

#include <functional>
#include <vector>

#include "components/drawable.h"
#include "components/generic.h"
#include "core/entity.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/texture.h"

namespace ptgn {

class Scene;
class Camera;
class Manager;
class RenderTarget;

enum class ResizeToResolution {
	Logical,
	Physical
};

namespace impl {

class RenderData;

RenderTarget AddRenderTargetComponents(
	const Entity& entity, Manager& manager, const V2_int& size, const Color& clear_color,
	TextureFormat texture_format
);

struct DisplayList {
	std::vector<Entity> entities;
};

struct ClearColor : public ColorComponent {
	using ColorComponent::ColorComponent;

	ClearColor() : ColorComponent{ color::Transparent } {}
};

struct LogicalRenderTargetResizeScript :
	public Script<LogicalRenderTargetResizeScript, LogicalResolutionScript> {
	void OnLogicalResolutionChanged() override;
};

struct PhysicalRenderTargetResizeScript :
	public Script<PhysicalRenderTargetResizeScript, PhysicalResolutionScript> {
	void OnPhysicalResolutionChanged() override;
};

} // namespace impl

// Each render target is initialized with a window camera.
class RenderTarget : public Entity {
public:
	// A default render target will result in the screen being used as the render target.
	RenderTarget() = default;
	RenderTarget(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// @return Unscaled size of the entire texture in pixels.
	[[nodiscard]] V2_int GetTextureSize() const;

	// @return Unscaled size of the cropped texture in pixels.
	[[nodiscard]] V2_int GetSize() const;

	// @return Scaled size of the cropped texture in pixels.
	[[nodiscard]] V2_float GetDisplaySize() const;

	void ClearDisplayList();
	void AddToDisplayList(Entity& entity);
	void RemoveFromDisplayList(Entity& entity);

	[[nodiscard]] const std::vector<Entity>& GetDisplayList() const;

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

	// Clear the render target. This function will bind the render target's frame buffer.
	void Clear() const;

	// Clear the render target to a specified color without modifying its internally stored clear
	// color. This function will bind the render target's frame buffer.
	void ClearToColor(const Color& color) const;

	// WARNING: This function is slow and should be
	// primarily used for debugging render targets.
	// @param coordinate Pixel coordinate from [0, size).
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture ids.
	// @return Color value of the given pixel.
	// Note: Only RGB/RGBA format textures supported.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate, bool restore_bind_state = true) const;

	// WARNING: This function is slow and should be
	// primarily used for debugging render targets.
	// @param callback Function to be called for each pixel.
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture ids.
	// Note: Only RGB/RGBA format textures supported.
	void ForEachPixel(
		const std::function<void(V2_int, Color)>& callback, bool restore_bind_state = true
	) const;

	void Resize(const V2_int& size);

private:
	friend class impl::RenderData;
	friend class Scene;
	friend RenderTarget impl::AddRenderTargetComponents(
		const Entity& entity, Manager& manager, const V2_int& size, const Color& clear_color,
		TextureFormat texture_format
	);

	// Scene uses vector directly when adding to display list instead of AddToDisplayList. This
	// avoids adding a render target to each scene entity.
	[[nodiscard]] std::vector<Entity>& GetDisplayList();
};

PTGN_DRAWABLE_REGISTER(RenderTarget);

// Create a render target with a custom size.
// @param size The size of the render target.
// @param clear_color The background color of the render target.
RenderTarget CreateRenderTarget(
	Manager& manager, const V2_int& size, const Color& clear_color = color::Transparent,
	TextureFormat texture_format = TextureFormat::RGBA8888
);

// Create a render target that is continuously sized to the specified resolution.
// @param clear_color The background color of the render target.
RenderTarget CreateRenderTarget(
	Manager& manager, ResizeToResolution resize_to_resolution = ResizeToResolution::Physical,
	const Color& clear_color	 = color::Transparent,
	TextureFormat texture_format = TextureFormat::RGBA8888
);

} // namespace ptgn