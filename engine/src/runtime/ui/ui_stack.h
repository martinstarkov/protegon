#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_creation_context.h"
#include "serialization/serialize.h"

namespace ptgn {

// The policy applied to a screen when another screen covers it.
enum class OverlayMode {
	Disable,
	Hide
};
PTGN_REFLECT_ENUM(OverlayMode);

// These components contain authored data only. Navigation and preview state are transient.
struct UIStackData {
	std::string initial{};
	std::vector<std::string> history{};
	bool cleared{ false };
	// Editor only. Nullopt displays the runtime initial layer. An empty vector hides all layers.
	std::optional<std::vector<std::string>> preview_visible{};

	PTGN_REFLECT(UIStackData, initial)
};

struct UILayerData {
	std::string key{};
	OverlayMode overlay{ OverlayMode::Disable };
	bool show_disabled_style{ false };
	bool block_background_input{ false };

	PTGN_REFLECT(UILayerData, key, overlay, show_disabled_style, block_background_input)
};

// Component absence means inherit the containing layer's overlay mode.
struct OverlayOverride {
	OverlayMode overlay{ OverlayMode::Disable };

	PTGN_REFLECT_VALUE(OverlayOverride, overlay)
};

class UILayer : public Entity {
public:
	UILayer() = default;

	explicit UILayer(Entity entity) : Entity{ entity } {}

	void SetOverlayMode(OverlayMode mode);
	void SetShowDisabledStyle(bool enabled);
	void SetBlockBackgroundInput(bool enabled);
	[[nodiscard]] std::string_view GetKey() const;
};

class UIStack : public Entity {
public:
	UIStack() = default;

	explicit UIStack(Entity entity) : Entity{ entity } {}

	[[nodiscard]] UILayer AddLayer(std::string_view key);
	[[nodiscard]] UILayer GetLayer(std::string_view key) const;
	[[nodiscard]] std::vector<UILayer> GetLayers() const;
	void SetInitialLayer(std::string_view key);
	[[nodiscard]] std::string_view GetInitialLayer() const;
	[[nodiscard]] std::string_view GetCurrentLayer() const;
	[[nodiscard]] std::vector<std::string> GetHistory() const;
	[[nodiscard]] std::size_t GetDepth() const;
	[[nodiscard]] bool CanPop() const;

	bool Push(std::string_view key);
	bool Pop();
	bool Replace(std::string_view key);
	bool PopTo(std::string_view key);
	bool Set(std::string_view key);
	void Reset();
	void Clear();

	// Editor only; independent visibility toggles can display multiple layers.
	// Nullopt restores the initial layer. Preview state is never serialized.
	void SetLayerPreviewVisible(std::string_view key, bool visible);
	[[nodiscard]] bool IsLayerPreviewVisible(std::string_view key) const;
	void ResetLayerPreview();
};

[[nodiscard]] UIStack CreateUIStack(EntityCreationContext context);
[[nodiscard]] UILayer CreateUILayer(EntityCreationContext context, std::string_view key);

// Finds the containing screen/stack through entity parentage, including managed UI parts.
[[nodiscard]] UILayer GetUILayer(Entity entity);
[[nodiscard]] UIStack GetUIStack(Entity entity);

// Effective UI state is computed without modifying Visible or Interactive components.
[[nodiscard]] bool IsUIStackVisible(Entity entity);
[[nodiscard]] bool IsUIStackInteractive(Entity entity);
[[nodiscard]] bool UseUIDisabledStyle(Entity entity);

// Depth offset for entities in a stack. A screen higher in the stack has higher priority.
[[nodiscard]] int GetUIStackDepthOffset(Entity entity);

} // namespace ptgn
