#pragma once

#include <functional>
#include <type_traits>
#include <unordered_map>

#include "event/event.h"
#include "event/events.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "utility/handle.h"
#include "utility/type_traits.h"

namespace ptgn {

class ToggleButtonGroup;

enum class ButtonState : std::uint8_t {
	Default = 0,
	Hover	= 1,
	Pressed = 2
};

using ButtonCallback = std::function<void()>;

enum class ButtonProperty : std::size_t {
	Texture,		  // Type: Texture
	BackgroundColor,  // Type: Color
	TextureTintColor, // Type: Color
	Text,			  // Type: Text
	TextColor,		  // Type: Color
	BorderColor,	  // Type: Color
	TextAlignment,	  // Type: TextAlignment
	TextSize,		  // Type: V2_float
	RenderLayer,	  // Type: std::size_t
	Visibility,		  // Type: bool
	Toggleable,		  // Type: bool
	Bordered,		  // Type: bool
	Toggled,		  // Type: bool
	LineThickness,	  // Type: float
	BorderThickness,  // Type: float
	OnHoverStart,	  // Type: ButtonCallback
	OnHoverStop,	  // Type: ButtonCallback
	OnActivate,		  // Type: ButtonCallback
	OnDisable,		  // Type: ButtonCallback
	OnEnable,		  // Type: ButtonCallback
	OnToggle,		  // Type: ButtonCallback
};

using TextAlignment = Origin;

namespace impl {

enum class InternalButtonState : std::size_t {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

enum class ButtonResourceState : uint8_t {
	Default			= 0b0000,
	Hover			= 0b0001,
	Pressed			= 0b0010,
	Disabled		= 0b0100,
	ToggledDefault	= 0b1000,
	ToggledHover	= 0b1001,
	ToggledPressed	= 0b1010,
	ToggledDisabled = 0b1100,
};

[[nodiscard]] ButtonResourceState GetButtonResourceState(
	ButtonState button_state, bool toggled, bool disabled
);

template <typename S>
[[nodiscard]] static auto GetResource(
	ButtonState state, bool toggled, bool disabled, const S& resource
) {
	if constexpr (tt::is_map_type_v<S>) {
		auto s{ GetButtonResourceState(state, toggled, disabled) };
		if (auto it{ resource.find(s) }; it != resource.end()) {
			return it->second;
		}
		return typename std::remove_reference_t<decltype(resource)>::mapped_type{};
	} else {
		return resource;
	}
}

template <typename S>
[[nodiscard]] static auto& GetResource(
	ButtonState state, bool toggled, bool disabled, S& resource
) {
	if constexpr (tt::is_map_type_v<S>) {
		auto s{ GetButtonResourceState(state, toggled, disabled) };
		if (auto it{ resource.find(s) }; it != resource.end()) {
			return it->second;
		}
		return resource[s];
	} else {
		return resource;
	}
}

class ButtonInstance {
public:
	ButtonInstance();
	~ButtonInstance();

	void Activate();
	void StartHover();
	void StopHover();
	void Toggle();
	void Enable();
	void Disable();

	[[nodiscard]] bool InsideRect(const V2_int& position) const;

	void OnMouseEvent(MouseEvent type, const Event& event);
	void OnMouseMove(const MouseMoveEvent& e);
	void OnMouseMoveOutside(const MouseMoveEvent& e);
	void OnMouseEnter(const MouseMoveEvent& e);
	void OnMouseLeave(const MouseMoveEvent& e);
	void OnMouseDown(const MouseDownEvent& e);
	void OnMouseDownOutside(const MouseDownEvent& e);
	void OnMouseUp(const MouseUpEvent& e);
	void OnMouseUpOutside(const MouseUpEvent& e);

	void MouseMotionUpdate(const V2_int& current, const V2_int& previous, const MouseMoveEvent& e);

	// Simulates a mouse move event from infinity to the current mouse position to refresh the
	// button state.
	void RecheckState();

	bool enabled_{ true };
	Rect rect_;
	InternalButtonState button_state_{ InternalButtonState::IdleUp };

	ButtonCallback internal_on_activate_;
	ButtonCallback on_activate_;
	ButtonCallback on_hover_start_;
	ButtonCallback on_hover_stop_;
	ButtonCallback on_enable_;
	ButtonCallback on_disable_;
	ButtonCallback on_toggle_;

	std::size_t render_layer_{ 0 };

	// -1 for solid button.
	float line_thickness_{ -1.0f };
	float border_thickness_{ 1.0f };

	// If either axis of the text size is zero, it is stretched to fit the entire size of the
	// button rectangle (along that axis).
	V2_float text_size_;
	TextAlignment text_alignment_{ TextAlignment::Center };

	std::unordered_map<ButtonResourceState, Texture> textures_;
	std::unordered_map<ButtonResourceState, Color> texture_tint_colors_;
	std::unordered_map<ButtonResourceState, Color> bg_colors_;
	std::unordered_map<ButtonResourceState, Text> texts_;
	std::unordered_map<ButtonResourceState, Color> text_colors_;
	std::unordered_map<ButtonResourceState, Color> border_colors_;

	bool visibility_{ true };
	bool toggleable_{ false };
	bool bordered_{ false };
	bool toggled_{ false };

	template <ButtonProperty T>
	auto& GetResource() {
		if constexpr (T == ButtonProperty::Texture) {
			return textures_;
		} else if constexpr (T == ButtonProperty::Text) {
			return texts_;
		} else if constexpr (T == ButtonProperty::BackgroundColor) {
			return bg_colors_;
		} else if constexpr (T == ButtonProperty::TextColor) {
			return text_colors_;
		} else if constexpr (T == ButtonProperty::TextureTintColor) {
			return texture_tint_colors_;
		} else if constexpr (T == ButtonProperty::BorderColor) {
			return border_colors_;
		} else if constexpr (T == ButtonProperty::TextAlignment) {
			return text_alignment_;
		} else if constexpr (T == ButtonProperty::TextSize) {
			return text_size_;
		} else if constexpr (T == ButtonProperty::Visibility) {
			return visibility_;
		} else if constexpr (T == ButtonProperty::Toggleable) {
			return toggleable_;
		} else if constexpr (T == ButtonProperty::Bordered) {
			return bordered_;
		} else if constexpr (T == ButtonProperty::Toggled) {
			return toggled_;
		} else if constexpr (T == ButtonProperty::LineThickness) {
			return line_thickness_;
		} else if constexpr (T == ButtonProperty::BorderThickness) {
			return border_thickness_;
		} else if constexpr (T == ButtonProperty::RenderLayer) {
			return render_layer_;
		} else if constexpr (T == ButtonProperty::OnActivate) {
			return on_activate_;
		} else if constexpr (T == ButtonProperty::OnDisable) {
			return on_disable_;
		} else if constexpr (T == ButtonProperty::OnEnable) {
			return on_enable_;
		} else if constexpr (T == ButtonProperty::OnHoverStart) {
			return on_hover_start_;
		} else if constexpr (T == ButtonProperty::OnHoverStop) {
			return on_hover_stop_;
		} else if constexpr (T == ButtonProperty::OnToggle) {
			return on_toggle_;
		} else {
			PTGN_ERROR("Invalid button property");
		}
	}
};

} // namespace impl

class Button : public Handle<impl::ButtonInstance> {
public:
	Button();
	explicit Button(const Rect& rect);
	~Button() override = default;

	void Draw() const;

	// These functions cause button to stop responding to events.
	[[nodiscard]] bool IsEnabled() const;
	Button& SetEnabled(bool enabled);

	// These allow for manually triggering button callback events.
	Button& Activate();
	Button& StartHover();
	Button& StopHover();
	Button& Toggle();
	Button& Disable();
	Button& Enable();

	[[nodiscard]] Rect GetRect() const;
	Button& SetRect(const Rect& new_rect);

	[[nodiscard]] ButtonState GetState() const;
	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

	template <ButtonProperty Property>
	[[nodiscard]] auto Get(
		ButtonState state = ButtonState::Default, bool toggled = false, bool disabled = false
	) const {
		const auto& resource{ const_cast<Button&>(*this).Handle::Get().GetResource<Property>() };
		return impl::GetResource(state, toggled, disabled, resource);
	}

	template <ButtonProperty Property>
	[[nodiscard]] auto GetCurrent() const {
		auto& i{ Handle::Get() };
		return Get<Property>(GetState(), i.toggled_, !i.enabled_);
	}

	template <ButtonProperty Property, typename T>
	Button& Set(
		const T& value, ButtonState state = ButtonState::Default, bool toggled = false,
		bool disabled = false
	) {
		auto& i{ Handle::Get() };
		auto& resource{ impl::GetResource(state, toggled, disabled, i.GetResource<Property>()) };
		using S = std::remove_reference_t<decltype(resource)>;
		if constexpr (std::is_constructible_v<ButtonCallback, S>) {
			resource = ButtonCallback(value);
		} else {
			static_assert(
				std::is_same_v<S, T>,
				"Cannot set button value to type which does not match type of property"
			);
			resource = value;
		}
		i.RecheckState();
		return *this;
	}

private:
	friend class ToggleButtonGroup;

	void SetInternalOnActivate(const ButtonCallback& internal_on_activate);
};

class ToggleButtonGroup : public MapManager<Button> {
public:
	using MapManager::MapManager;

	template <typename TKey, typename... TArgs, tt::constructible<Button, TArgs...> = true>
	Button& Load(const TKey& key, TArgs&&... constructor_args) {
		auto k{ GetInternalKey(key) };
		Button& button{ MapManager::Load(key, Button{ std::forward<TArgs>(constructor_args)... }) };
		// Toggle all other buttons when one is pressed.
		button.SetInternalOnActivate([this, &button]() {
			ForEachValue([](Button& b) { b.Set<ButtonProperty::Toggled>(false); });
			button.Set<ButtonProperty::Toggled>(true);
		});
		return button;
	}

	void Draw() const;
};

} // namespace ptgn