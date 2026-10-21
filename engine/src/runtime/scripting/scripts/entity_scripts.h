#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct SetVisibleScript : public Script {
	bool visible{ true };
	SetVisibleScript() = default;

	explicit SetVisibleScript(bool set_visible) : visible{ set_visible } {}

	void OnStart() override;
	PTGN_REFLECT(SetVisibleScript, visible)
};

struct SetEnabledScript : public Script {
	std::string component{ "Interactive" };
	bool enabled{ true };
	SetEnabledScript() = default;

	SetEnabledScript(std::string component_name, bool enable = true) :
		component{ std::move(component_name) }, enabled{ enable } {}

	void OnStart() override;
	PTGN_REFLECT(SetEnabledScript, component, enabled)
};

struct AddComponentsScript : public Script {
	std::vector<ComponentDefinition> components{};
	void OnStart() override;
	PTGN_REFLECT(AddComponentsScript, components)
};

struct RemoveComponentsScript : public Script {
	std::vector<std::string> components{};
	void OnStart() override;
	PTGN_REFLECT(RemoveComponentsScript, components)
};

struct EntityDestroyEntityScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(EntityDestroyEntityScript, option)
};

struct EntitySpawnPrefabScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EntitySpawnPrefabScript, text)
};

struct EntitySetColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };

	PTGN_REFLECT(EntitySetColorScript, selected_color)
};

struct EntitySetDepthScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(EntitySetDepthScript, number)
};

struct EntitySetPositionScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(EntitySetPositionScript, vector)
};

struct EntityAddPositionScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(EntityAddPositionScript, vector)
};

struct EntitySetRotationScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(EntitySetRotationScript, number)
};

struct EntityAddRotationScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(EntityAddRotationScript, number)
};

struct EntitySetScaleScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(EntitySetScaleScript, vector)
};

struct EntityMultiplyScaleScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(EntityMultiplyScaleScript, vector)
};

struct EntitySetRenderMaskScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(EntitySetRenderMaskScript, index)
};

struct EntitySetBlendModeScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EntitySetBlendModeScript, text)
};

struct EntitySetOriginScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EntitySetOriginScript, text)
};

struct EntitySetParentScript : public Script {
	void OnStart() override;
	std::string text{};
	bool option{ true };

	PTGN_REFLECT(EntitySetParentScript, text, option)
};

struct EntityRemoveParentScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(EntityRemoveParentScript)
};

} // namespace ptgn
