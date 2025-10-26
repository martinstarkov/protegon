#pragma once

#include <concepts>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/ecs/components/drawable.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/entity.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/type_info.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/materials/texture.h"
#include "serialization/json/serializable.h"

namespace ptgn {

class Scene;
class Camera;
class Manager;
class RenderTarget;

enum class ResizeMode {
	GameSize,
	DisplaySize
};

enum class FilterType {
	Pre,
	Post
};

namespace impl {

class RenderData;

RenderTarget AddRenderTargetComponents(
	const Entity& entity, Manager& manager, const V2_int& render_target_size, bool game_size_camera,
	const Color& clear_color, TextureFormat texture_format
);

struct DisplayList {
	std::vector<Entity> entities;
};

struct ClearColor : public ColorComponent {
	using ColorComponent::ColorComponent;

	ClearColor() : ColorComponent{ color::Transparent } {}
};

struct GameResizeScript : public Script<GameResizeScript, GameSizeScript> {
	void OnGameSizeChanged() override;
};

struct DisplayResizeScript : public Script<DisplayResizeScript, DisplaySizeScript> {
	void OnDisplaySizeChanged() override;
};

template <typename T>
concept DrawFilterType = requires(RenderTarget& render_target, FilterType type) {
	{ T::Filter(render_target, type) } -> std::same_as<void>;
};

class IDrawFilter {
public:
	IDrawFilter() = default;

	IDrawFilter(std::string_view name) : hash{ Hash(name) } {}

	using FilterFunc = void (*)(RenderTarget&, FilterType);

	static auto& data() {
		static std::unordered_map<std::size_t, FilterFunc> s;
		return s;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(IDrawFilter, hash)

	std::size_t hash{ 0 };
};

template <DrawFilterType T>
class DrawFilterRegistrar {
	friend RenderTarget;

	friend T;

	static bool RegisterDrawFilterFunction() {
		constexpr auto name{ type_name<T>() };

		IDrawFilter::data()[Hash(name)] = &T::Filter;

		return true;
	}

	static bool registered_filter;

	DrawFilterRegistrar() {
		(void)registered_filter;
	}
};

template <DrawFilterType T>
bool DrawFilterRegistrar<T>::registered_filter =
	DrawFilterRegistrar<T>::RegisterDrawFilterFunction();

// @return render_target
RenderTarget& SetDrawFilterImpl(RenderTarget& render_target, std::string_view filter_name);

} // namespace impl

#define PTGN_DRAW_FILTER_REGISTER(Type) template class impl::DrawFilterRegistrar<Type>

// Each render target is initialized with a window camera.
class RenderTarget : public Entity {
public:
	// A default render target will result in the screen being used as the render target.
	RenderTarget() = default;
	RenderTarget(const Entity& entity);

	static void Draw(const Entity& entity);

	// Interface function for filtering the display list prior to drawing its entities to the render
	// target.
	static void Filter(
		[[maybe_unused]] RenderTarget& render_target, [[maybe_unused]] FilterType type
	) {}

	// @return Unscaled size of the entire texture in pixels.
	[[nodiscard]] V2_int GetTextureSize() const;

	// @return Unscaled size of the cropped texture in pixels.
	[[nodiscard]] V2_int GetSize() const;

	// @return Scaled size of the cropped texture in pixels.
	[[nodiscard]] V2_float GetDisplaySize() const;

	[[nodiscard]] const Camera& GetCamera() const;
	[[nodiscard]] Camera& GetCamera();

	void ClearDisplayList();
	void AddToDisplayList(Entity entity);
	void RemoveFromDisplayList(Entity entity);

	[[nodiscard]] const std::vector<Entity>& GetDisplayList() const;

	[[nodiscard]] std::vector<Entity>& GetDisplayList();

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

	// @return render_target.
	template <impl::DrawFilterType T>
	RenderTarget& SetDrawFilter() {
		return impl::SetDrawFilterImpl(*this, type_name<T>());
	}

	[[nodiscard]] bool HasDrawFilter() const;

	// @return render_target.
	RenderTarget& RemoveDrawFilter();

private:
	friend class impl::RenderData;
	friend class Scene;
	friend RenderTarget impl::AddRenderTargetComponents(
		const Entity& entity, Manager& manager, const V2_int& render_target_size,
		bool game_size_camera, const Color& clear_color, TextureFormat texture_format
	);
};

PTGN_DRAWABLE_REGISTER(RenderTarget);
PTGN_DRAW_FILTER_REGISTER(RenderTarget);

// Create a render target with a custom size.
// @param size The size of the render target and its camera viewport.
// @param clear_color The background color of the render target.
// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
RenderTarget CreateRenderTarget(
	Manager& manager, const V2_int& size, const Color& clear_color = color::Transparent,
	TextureFormat texture_format = TextureFormat::RGBA8888
);

// Create a render target that is continuously sized to the specified resolution.
// @param resize_to_resolution Which resolution the render target automatically resizes to.
// @param clear_color The background color of the render target.
// @param game_size_camera If true, render target camera is set to auto resize to the game size
// instead of to the render target size.
// @param Texture format of the render target texture. Mostly used for enabling HDR targets.
RenderTarget CreateRenderTarget(
	Manager& manager, ResizeMode resize_to_resolution = ResizeMode::DisplaySize,
	bool game_size_camera = true, const Color& clear_color = color::Transparent,
	TextureFormat texture_format = TextureFormat::RGBA8888
);

} // namespace ptgn