#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/strong_string.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/key_hash.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "serialization/serialize.h"

namespace ptgn {

class Button;
class ToggleButton;
class ToggleButtonGroup;
class Scene;

namespace event {

struct ToggleButtonToggle;

} // namespace event

namespace impl {

struct ToggleButtonData {
	bool toggled{ false };

	PTGN_REFLECT(ToggleButtonData, toggled)
};

struct ToggleButtonGroupKey : public StrongString<ToggleButtonGroupKey> {
	using StrongString::StrongString;

	constexpr ToggleButtonGroupKey() = default;

	friend std::ostream& operator<<(std::ostream& os, const ToggleButtonGroupKey& key) {
		os << key.value;
		return os;
	}

	PTGN_REFLECT_VALUE(ToggleButtonGroupKey, value)
};

struct ToggleButtonGroupData {
	bool always_active{ true };
	std::optional<ToggleButtonGroupKey> active;

	PTGN_REFLECT(ToggleButtonGroupData, always_active, active)
};

/// @brief Marker for direct child toggle buttons belonging to a toggle group.
struct ToggleButtonGroupItem {
	ToggleButtonGroupKey key;

	PTGN_REFLECT_VALUE(ToggleButtonGroupItem, key)
};

struct ToggleButtonSystem {
	/// @brief Makes ToggleButtonData entities ordinary buttons automatically.
	static void Prepare(Scene& scene);

	/// @brief Applies standalone or grouped toggle behavior when ButtonPress is emitted.
	static void OnEvent(Entity entity, Event event);

private:
	static void OnButtonPress(Entity entity);
};

} // namespace impl

class ToggleButton : public Button {
public:
	using Button::Button;

	[[nodiscard]] bool IsToggled() const;

	ToggleButton& SetToggled(bool toggled);
	ToggleButton& Toggle();

	template <EventCallbackInvocable<event::ToggleButtonToggle> F>
	ToggleButton& OnToggle(F&& callback) {
		AddScript<impl::EventScript<event::ToggleButtonToggle>>(
			*this, impl::MakeEventCallback<event::ToggleButtonToggle>(std::forward<F>(callback))
		);
		return *this;
	}
};

class ToggleButtonGroup : public Entity {
public:
	ToggleButtonGroup() = default;
	explicit ToggleButtonGroup(Entity entity);

	void SetAlwaysOneActive(bool always_active, std::optional<std::string_view> button_key = {});

	ToggleButton Add(std::string_view button_key, ToggleButton toggle_button);

	void Remove(std::string_view button_key) const;

	void SetActive(std::string_view button_key);

	[[nodiscard]] std::optional<ToggleButton> GetActive() const;
	[[nodiscard]] std::vector<ToggleButton> GetButtons() const;

private:
	friend struct impl::ToggleButtonSystem;

	void SetActiveKey(impl::ToggleButtonGroupKey key);
};

namespace event {

struct ToggleButtonToggle {
	ToggleButton button;
	bool toggled{ false };
};

} // namespace event

ToggleButton CreateToggleButton(
	Scene& scene, Transform transform, V2_float size, Origin origin = Origin::Center,
	bool toggled = false
);

ToggleButton CreateToggleButton(
	Scene& scene, Transform transform, float radius, Origin origin = Origin::Center,
	bool toggled = false
);

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

} // namespace ptgn
