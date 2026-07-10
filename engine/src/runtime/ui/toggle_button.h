#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
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

	PTGN_SERIALIZE(ToggleButtonData, toggled)
};

class ToggleButtonScript : public Script {
public:
	ToggleButtonScript() = default;

	void OnEvent(Event event) override;

private:
	void OnButtonPress() const;
};

struct ToggleButtonGroupKey : public KeyHash {
	using KeyHash::KeyHash;
};

struct ToggleButtonGroupData {
	bool always_active{ true };
	std::optional<ToggleButtonGroupKey> active;
};

/// @brief Marker for direct child toggle buttons belonging to a toggle group.
struct ToggleButtonGroupItem {
	ToggleButtonGroupKey key;
};

class ToggleButtonGroupScript;

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
	friend class impl::ToggleButtonGroupScript;

	void AddToggleScript(ToggleButton toggle_button) const;
	void SetActiveKey(impl::ToggleButtonGroupKey key);
};

namespace impl {

class ToggleButtonGroupScript : public Script {
public:
	ToggleButtonGroupScript() = default;
	explicit ToggleButtonGroupScript(ToggleButtonGroup group);

	void OnEvent(Event event) override;

private:
	void OnButtonPress();

	ToggleButtonGroup toggle_button_group_;
};

} // namespace impl

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