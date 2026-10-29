#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/ecs/entity_group.h"
#include "runtime/ecs/uuid.h"
#include "serialization/serialize.h"

namespace ptgn {

// Groups are discovered from direct hierarchy children, not registered by name.
// UUID references preserve navigation through renaming and are checked against the hierarchy.
struct GroupStackData {
	UUID initial{ 0 };
	std::vector<UUID> history{};
	bool cleared{ false };
	// Editor only: nullopt displays the initial group; an empty vector hides all groups.
	std::optional<std::vector<UUID>> preview_visible{};

	PTGN_REFLECT(GroupStackData, initial)
};

struct OverlayOverride {
	OverlayMode overlay{ OverlayMode::Disable };

	PTGN_REFLECT_VALUE(OverlayOverride, overlay)
};

class GroupEntity : public Entity {
public:
	GroupEntity() = default;

	explicit GroupEntity(Entity entity) : Entity{ entity } {}

	void SetOverlayMode(OverlayMode mode);
	void SetShowDisabledStyle(bool enabled);
	void SetBlockBackgroundInput(bool enabled);
};

class GroupStack : public Entity {
public:
	GroupStack() = default;

	explicit GroupStack(Entity entity) : Entity{ entity } {}

	[[nodiscard]] GroupEntity AddGroup(std::string_view name);
	[[nodiscard]] GroupEntity GetGroup(UUID uuid) const;
	[[nodiscard]] std::vector<GroupEntity> GetGroups() const;
	void SetInitialGroup(Entity group);
	[[nodiscard]] GroupEntity GetInitialGroup() const;
	[[nodiscard]] GroupEntity GetCurrentGroup() const;
	[[nodiscard]] std::vector<UUID> GetHistory() const;
	[[nodiscard]] std::size_t GetDepth() const;
	[[nodiscard]] bool CanPop() const;

	bool Push(UUID group);

	bool Push(Entity group) {
		return group && group.Has<UUID>() && Push(group.Get<UUID>());
	}

	bool Pop();
	bool Replace(UUID group);

	bool Replace(Entity group) {
		return group && group.Has<UUID>() && Replace(group.Get<UUID>());
	}

	bool PopTo(UUID group);

	bool PopTo(Entity group) {
		return group && group.Has<UUID>() && PopTo(group.Get<UUID>());
	}

	bool Set(UUID group);

	bool Set(Entity group) {
		return group && group.Has<UUID>() && Set(group.Get<UUID>());
	}

	void Reset();
	void Clear();

	void SetGroupPreviewVisible(Entity group, bool visible);
	void ShowOnlyGroupPreview(Entity group);
	[[nodiscard]] bool IsGroupPreviewVisible(Entity group) const;
	void ResetGroupPreview();
};

[[nodiscard]] GroupStack CreateGroupStack(EntityCreationContext context);
[[nodiscard]] GroupEntity CreateGroup(EntityCreationContext context, std::string_view name);

// An independent group is not a stack member until directly parented to a GroupStack.
[[nodiscard]] GroupEntity GetGroup(Entity entity);
[[nodiscard]] GroupStack GetGroupStack(Entity entity);

[[nodiscard]] bool IsGroupStackVisible(Entity entity);
[[nodiscard]] bool IsGroupStackInteractive(Entity entity);
[[nodiscard]] bool IsGroupStackActive(Entity entity);
[[nodiscard]] bool UseGroupDisabledStyle(Entity entity);
[[nodiscard]] int GetGroupStackDepthOffset(Entity entity);

} // namespace ptgn
