#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/math/transform.h"
#include "editor/editor_context.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/relatives.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "serialization/json/json.h"

namespace ptgn::editor::entity_target_demo {

enum class EntityTargetType : std::uint8_t {
	Entity,
	Components,
	Group,
	Query,
};

struct EntityReference {
	std::optional<UUID> uuid;
	std::string tag;
};

struct ComponentQueryCondition {
	std::string component;
	bool required{ true };
};

struct ComponentQueryGroup {
	std::vector<ComponentQueryCondition> conditions;
};

struct ComponentEntityQuery {
	std::vector<ComponentQueryGroup> groups;
};

struct GroupEntityQuery {
	std::string group;
};

struct RegisteredEntityQueryReference {
	std::string key;
};

struct EntityTarget {
	EntityTargetType type{ EntityTargetType::Entity };
	EntityReference entity;
	ComponentEntityQuery components;
	GroupEntityQuery group;
	RegisteredEntityQueryReference query;
};

struct EntityQueryContext {
	Scene& scene;
	Entity owner;
	Entity target;
};

using EntityQueryCallback = bool (*)(const EntityQueryContext&);

struct RegisteredEntityQuery {
	std::string key;
	std::string label;
	std::string group;
	std::string description;
	EntityQueryCallback evaluate{ nullptr };
};

class EntityQueryRegistry {
public:
	static void Register(RegisteredEntityQuery query) {
		if (query.key.empty() || !query.evaluate) {
			return;
		}

		auto& queries{ Storage() };

		if (auto it{
				std::ranges::find(
					queries,
					query.key,
					&RegisteredEntityQuery::key
				)
			};
			it != queries.end()) {
			*it = std::move(query);
			return;
		}

		queries.push_back(std::move(query));
	}

	[[nodiscard]] static const RegisteredEntityQuery* Find(
		std::string_view key
	) {
		const auto& queries{ Storage() };

		const auto it{
			std::ranges::find(
				queries,
				key,
				&RegisteredEntityQuery::key
			)
		};

		return it == queries.end()
			? nullptr
			: std::addressof(*it);
	}

	[[nodiscard]] static const std::vector<RegisteredEntityQuery>& Queries() {
		return Storage();
	}

private:
	[[nodiscard]] static std::vector<RegisteredEntityQuery>& Storage() {
		static std::vector<RegisteredEntityQuery> queries;
		return queries;
	}
};

template <auto Function>
class AutoEntityQueryRegistration {
public:
	AutoEntityQueryRegistration(
		std::string key,
		std::string label,
		std::string group,
		std::string description
	) {
		EntityQueryRegistry::Register(
			RegisteredEntityQuery{
				.key = std::move(key),
				.label = std::move(label),
				.group = std::move(group),
				.description = std::move(description),
				.evaluate = Function,
			}
		);
	}
};

#define PTGN_DEMO_CONCAT_IMPL(a, b) a##b
#define PTGN_DEMO_CONCAT(a, b) PTGN_DEMO_CONCAT_IMPL(a, b)

#define PTGN_DEMO_REGISTER_ENTITY_QUERY(Key, Function, Label, Group, Description) \
	[[maybe_unused]] const AutoEntityQueryRegistration<Function>                  \
		PTGN_DEMO_CONCAT(kEntityQueryRegistration_, __COUNTER__){                 \
			Key, Label, Group, Description                                        \
		}

struct RegisteredEntityGroup {
	std::string key;
	std::string label;
	std::vector<UUID> members;
};

class EntityGroupRegistry {
public:
	static void Register(
		std::string key,
		std::string label
	) {
		if (key.empty()) {
			return;
		}

		auto& groups{ Storage() };

		if (auto it{
				std::ranges::find(
					groups,
					key,
					&RegisteredEntityGroup::key
				)
			};
			it != groups.end()) {
			it->label = std::move(label);
			return;
		}

		groups.push_back(
			RegisteredEntityGroup{
				.key = std::move(key),
				.label = std::move(label),
			}
		);
	}

	static void Add(
		std::string_view key,
		Entity entity
	) {
		if (!entity) {
			return;
		}

		auto* group{ FindMutable(key) };

		if (!group) {
			return;
		}

		const UUID uuid{ entity.Get<UUID>() };

		if (!std::ranges::contains(
				group->members,
				uuid
			)) {
			group->members.push_back(uuid);
		}
	}

	static void Remove(
		std::string_view key,
		Entity entity
	) {
		if (!entity) {
			return;
		}

		auto* group{ FindMutable(key) };

		if (!group) {
			return;
		}

		std::erase(
			group->members,
			entity.Get<UUID>()
		);
	}

	[[nodiscard]] static const RegisteredEntityGroup* Find(
		std::string_view key
	) {
		const auto& groups{ Storage() };

		const auto it{
			std::ranges::find(
				groups,
				key,
				&RegisteredEntityGroup::key
			)
		};

		return it == groups.end()
			? nullptr
			: std::addressof(*it);
	}

	[[nodiscard]] static const std::vector<RegisteredEntityGroup>& Groups() {
		return Storage();
	}

private:
	[[nodiscard]] static RegisteredEntityGroup* FindMutable(
		std::string_view key
	) {
		auto& groups{ Storage() };

		const auto it{
			std::ranges::find(
				groups,
				key,
				&RegisteredEntityGroup::key
			)
		};

		return it == groups.end()
			? nullptr
			: std::addressof(*it);
	}

	[[nodiscard]] static std::vector<RegisteredEntityGroup>& Storage() {
		static std::vector<RegisteredEntityGroup> groups;
		return groups;
	}
};

struct DemoState {
	EntityReference owner;
	EntityTarget target;

	std::string hierarchy_filter;
	std::string component_filter;
	std::string group_filter;
	std::string query_filter;

	bool components_initialized{ false };
};

namespace {

constexpr std::size_t kMaxHierarchyDepth{ 256 };
constexpr float kEntityHierarchyHeight{ 300.0f };
constexpr float kEntityModeMinWidth{ 420.0f };
constexpr float kComponentsModeMinWidth{ 540.0f };
constexpr float kGroupsModeMinWidth{ 360.0f };
constexpr float kQueriesModeMinWidth{ 400.0f };
constexpr float kTargetPopupMaxWidth{ 720.0f };

[[nodiscard]] char LowerAscii(char c) {
	if (c >= 'A' && c <= 'Z') {
		return static_cast<char>(c - 'A' + 'a');
	}

	return c;
}

[[nodiscard]] std::string ToLower(std::string value) {
	for (char& c : value) {
		c = LowerAscii(c);
	}

	return value;
}

[[nodiscard]] std::string TrimWhitespace(std::string value) {
	const auto first{
		std::ranges::find_if(
			value,
			[](char c) {
				return !std::isspace(
					static_cast<unsigned char>(c)
				);
			}
		)
	};

	if (first == value.end()) {
		return {};
	}

	const auto last{
		std::ranges::find_if(
			value | std::views::reverse,
			[](char c) {
				return !std::isspace(
					static_cast<unsigned char>(c)
				);
			}
		).base()
	};

	return std::string{ first, last };
}

[[nodiscard]] bool ContainsCaseInsensitive(
	std::string_view text,
	std::string_view filter
) {
	if (filter.empty()) {
		return true;
	}

	if (filter.size() > text.size()) {
		return false;
	}

	for (std::size_t i{ 0 };
		 i + filter.size() <= text.size();
		 ++i) {
		bool matches{ true };

		for (std::size_t j{ 0 };
			 j < filter.size();
			 ++j) {
			if (LowerAscii(text[i + j]) !=
				LowerAscii(filter[j])) {
				matches = false;
				break;
			}
		}

		if (matches) {
			return true;
		}
	}

	return false;
}

[[nodiscard]] std::string_view ComponentDisplayName(
	const RegisteredComponent& component
) {
	std::string_view name{ component.name };

	if (const auto separator{ name.rfind("::") };
		separator != std::string_view::npos) {
		name.remove_prefix(separator + 2);
	}

	return name;
}

[[nodiscard]] std::string EntityDisplayName(
	Entity entity
) {
	if (!entity) {
		return "Missing Entity";
	}

	const auto& tag{ entity.Get<Tag>().value };

	return tag.empty()
		? "Entity"
		: tag;
}

[[nodiscard]] std::string UUIDDisplayName(
	Entity entity
) {
	if (!entity) {
		return {};
	}

	json value =
		entity.Get<UUID>();

	if (value.is_string()) {
		return value.get<std::string>();
	}

	return value.dump();
}

void SetEntityReference(
	EntityReference& reference,
	Entity entity
) {
	if (!entity) {
		reference = {};
		return;
	}

	reference.uuid = entity.Get<UUID>();
	reference.tag = entity.Get<Tag>().value;
}

[[nodiscard]] Entity ResolveEntity(
	Scene& scene,
	const EntityReference& reference
) {
	if (!reference.uuid.has_value()) {
		return {};
	}

	for (Entity entity : scene.Entities()) {
		if (entity.Get<UUID>() ==
			reference.uuid.value()) {
			return entity;
		}
	}

	return {};
}

[[nodiscard]] std::vector<const RegisteredComponent*>
GetRegisteredComponents() {
	std::vector<const RegisteredComponent*> components;

	for (const auto& component :
		 ComponentRegistry::Components()) {
		components.push_back(
			std::addressof(component)
		);
	}

	std::ranges::sort(
		components,
		[](const auto* lhs, const auto* rhs) {
			return ComponentDisplayName(*lhs) <
				   ComponentDisplayName(*rhs);
		}
	);

	return components;
}

[[nodiscard]] bool IsTargetNotOwner(
	const EntityQueryContext& ctx
) {
	return ctx.target &&
		   ctx.target != ctx.owner;
}

[[nodiscard]] bool IsTargetRightOfOwner(
	const EntityQueryContext& ctx
) {
	if (!ctx.owner ||
		!ctx.target ||
		!ctx.owner.Has<Transform>() ||
		!ctx.target.Has<Transform>()) {
		return false;
	}

	return ctx.target.Get<Transform>().position.x >
		   ctx.owner.Get<Transform>().position.x;
}

[[nodiscard]] bool IsEnemyNamedTarget(
	const EntityQueryContext& ctx
) {
	if (!ctx.target) {
		return false;
	}

	return ctx.target.Get<Tag>()
		.value.starts_with("Enemy");
}

PTGN_DEMO_REGISTER_ENTITY_QUERY(
	"not_owner",
	IsTargetNotOwner,
	"Not Owner",
	"General",
	"Matches every candidate except the sequence owner."
);

PTGN_DEMO_REGISTER_ENTITY_QUERY(
	"right_of_owner",
	IsTargetRightOfOwner,
	"Right Of Owner",
	"Spatial",
	"Matches targets whose Transform position is to the right of the owner."
);

PTGN_DEMO_REGISTER_ENTITY_QUERY(
	"enemy_named",
	IsEnemyNamedTarget,
	"Enemy Named",
	"Gameplay",
	"Matches entities whose tag starts with \"Enemy\"."
);

void EnsureDemoGroupsRegistered() {
	static bool registered{ false };

	if (registered) {
		return;
	}

	registered = true;

	EntityGroupRegistry::Register(
		"enemies",
		"Enemies"
	);

	EntityGroupRegistry::Register(
		"boss_minions",
		"Boss Minions"
	);

	EntityGroupRegistry::Register(
		"explodable_props",
		"Explodable Props"
	);

	EntityGroupRegistry::Register(
		"ui",
		"UI"
	);
}

[[nodiscard]] bool EntityMatchesFilter(
	Entity entity,
	std::string_view filter_text
) {
	if (filter_text.empty()) {
		return true;
	}

	auto name{
		ToLower(
			entity.Get<Tag>().value
		)
	};

	auto filter{
		std::string{ filter_text }
	};

	bool has_name_include{ false };
	bool matched_name_include{ false };

	bool require_hidden{ false };
	bool require_shown{ false };

	std::size_t start{ 0 };

	while (start <= filter.size()) {
		const auto comma{
			filter.find(
				',',
				start
			)
		};

		auto token{
			comma == std::string::npos
				? filter.substr(start)
				: filter.substr(
					  start,
					  comma - start
				  )
		};

		token = ToLower(
			TrimWhitespace(
				std::move(token)
			)
		);

		if (!token.empty()) {
			if (token.front() == '*') {
				auto filter_name{
					TrimWhitespace(
						token.substr(1)
					)
				};

				if (filter_name == "hidden") {
					require_hidden = true;
				} else if (
					filter_name == "shown") {
					require_shown = true;
				}
			} else {
				const bool exclude{
					token.front() == '-'
				};

				auto needle{
					TrimWhitespace(
						exclude
							? token.substr(1)
							: token
					)
				};

				if (!needle.empty()) {
					const bool contains{
						name.find(needle) !=
						std::string::npos
					};

					if (exclude &&
						contains) {
						return false;
					}

					if (!exclude) {
						has_name_include = true;
						matched_name_include |=
							contains;
					}
				}
			}
		}

		if (comma == std::string::npos) {
			break;
		}

		start = comma + 1;
	}

	if (require_hidden &&
		require_shown) {
		return false;
	}

	const bool visible{
		IsVisible(entity)
	};

	if (require_hidden &&
		visible) {
		return false;
	}

	if (require_shown &&
		!visible) {
		return false;
	}

	return !has_name_include ||
		   matched_name_include;
}

[[nodiscard]] bool EntityOrDescendantMatchesFilter(
	Entity entity,
	std::string_view filter_text,
	std::size_t depth = 0
) {
	if (!entity ||
		depth >= kMaxHierarchyDepth) {
		return false;
	}

	if (EntityMatchesFilter(
			entity,
			filter_text
		)) {
		return true;
	}

	if (!HasChildren(entity)) {
		return false;
	}

	for (Entity child :
		 GetChildren(entity)) {
		if (EntityOrDescendantMatchesFilter(
				child,
				filter_text,
				depth + 1
			)) {
			return true;
		}
	}

	return false;
}

[[nodiscard]] std::vector<Entity>
GetRootEntities(Scene& scene) {
	std::vector<Entity> roots;

	for (Entity entity : scene.Entities()) {
		if (!HasParent(entity)) {
			roots.push_back(entity);
		}
	}

	SortByLocalDepth(roots);
	return roots;
}

void DrawHierarchyFilterTooltip() {
	if (!ImGui::IsItemHovered()) {
		return;
	}

	ImGui::BeginTooltip();

	ImGui::TextUnformatted(
		"Hierarchy filter syntax:"
	);

	ImGui::Separator();

	ImGui::TextUnformatted("player");
	ImGui::SameLine();
	ImGui::TextDisabled(
		"Name contains \"player\""
	);

	ImGui::TextUnformatted("-enemy");
	ImGui::SameLine();
	ImGui::TextDisabled(
		"Name does not contain \"enemy\""
	);

	ImGui::TextUnformatted("*shown");
	ImGui::SameLine();
	ImGui::TextDisabled(
		"Entity is visible"
	);

	ImGui::TextUnformatted("*hidden");
	ImGui::SameLine();
	ImGui::TextDisabled(
		"Entity is hidden"
	);

	ImGui::Spacing();

	ImGui::TextDisabled(
		"Separate filters with commas."
	);

	ImGui::TextDisabled(
		"Positive name filters use OR; all other filters must match."
	);

	ImGui::EndTooltip();
}

void DrawEntityHierarchyNode(
	Entity entity,
	const EntityReference& current,
	std::string_view filter,
	Entity& picked_entity,
	std::size_t depth = 0
) {
	if (!entity ||
		depth >= kMaxHierarchyDepth) {
		return;
	}

	if (!EntityOrDescendantMatchesFilter(
			entity,
			filter,
			depth
		)) {
		return;
	}

	std::vector<Entity> children;

	if (HasChildren(entity)) {
		children = GetChildren(entity);

		children.erase(
			std::remove_if(
				children.begin(),
				children.end(),
				[&](Entity child) {
					return !EntityOrDescendantMatchesFilter(
						child,
						filter,
						depth + 1
					);
				}
			),
			children.end()
		);

		SortByLocalDepth(children);
	}

	const bool has_visible_children{
		!children.empty()
	};

	const bool selected{
		current.uuid.has_value() &&
		entity.Get<UUID>() ==
			current.uuid.value()
	};

	ImGui::PushID(
		entity.Get<UUID>()
	);

	ImGuiTreeNodeFlags flags{
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen
	};

	if (selected) {
		flags |=
			ImGuiTreeNodeFlags_Selected;
	}

	if (!has_visible_children) {
		flags |=
			ImGuiTreeNodeFlags_Leaf |
			ImGuiTreeNodeFlags_NoTreePushOnOpen;
	} else if (!filter.empty()) {
		ImGui::SetNextItemOpen(
			true,
			ImGuiCond_Always
		);
	}

	const std::string label{
		EntityDisplayName(entity)
	};

	const bool open{
		ImGui::TreeNodeEx(
			"##EntityTarget",
			flags,
			"%s",
			label.c_str()
		)
	};

	if (ImGui::IsItemClicked(
			ImGuiMouseButton_Left
		)) {
		picked_entity = entity;
	}

	if (has_visible_children &&
		open) {
		for (Entity child :
			 children) {
			DrawEntityHierarchyNode(
				child,
				current,
				filter,
				picked_entity,
				depth + 1
			);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void DrawMiniHierarchy(
	Scene& scene,
	Entity owner,
	EntityReference& reference,
	DemoState& state
) {
	if (owner) {
		if (ImGui::Button(
				"Select Script Owner"
			)) {
			SetEntityReference(
				reference,
				owner
			);
		}

		ImGui::SameLine();

		Entity selected{
			ResolveEntity(
				scene,
				reference
			)
		};

		ImGui::TextDisabled(
			"%s",
			selected
				? EntityDisplayName(
					  selected
				  ).c_str()
				: "No entity selected"
		);
	}

	ImGui::SetNextItemWidth(
		-FLT_MIN
	);

	ImGui::InputTextWithHint(
		"##HierarchyFilter",
		"Filter: player, -enemy, *hidden, *shown",
		&state.hierarchy_filter
	);

	DrawHierarchyFilterTooltip();

	ImGui::BeginChild(
		"##EntityHierarchy",
		ImVec2{
			0.0f,
			kEntityHierarchyHeight
		},
		ImGuiChildFlags_Borders
	);

	Entity picked_entity;

	for (Entity root :
		 GetRootEntities(scene)) {
		DrawEntityHierarchyNode(
			root,
			reference,
			state.hierarchy_filter,
			picked_entity
		);
	}

	if (picked_entity) {
		SetEntityReference(
			reference,
			picked_entity
		);
	}

	ImGui::EndChild();
}

bool DrawComponentPicker(
	const char* id,
	std::string& component_name,
	DemoState& state
) {
	const auto* selected{
		component_name.empty()
			? nullptr
			: ComponentRegistry::Find(
				  std::string_view{
					  component_name
				  }
			  )
	};

	const std::string preview{
		selected
			? std::string{
				  ComponentDisplayName(
					  *selected
				  )
			  }
			: component_name.empty()
				? "Select Component"
				: component_name +
					  " (Missing)"
	};

	bool changed{ false };

	if (ImGui::Button(
			preview.c_str(),
			ImVec2{
				ImGui::GetContentRegionAvail().x,
				ImGui::GetFrameHeight()
			}
		)) {
		state.component_filter.clear();

		ImGui::OpenPopup(id);
	}

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{
			320.0f,
			0.0f
		},
		ImVec2{
			440.0f,
			420.0f
		}
	);

	if (!ImGui::BeginPopup(id)) {
		return changed;
	}

	ImGui::SetNextItemWidth(
		-FLT_MIN
	);

	ImGui::InputTextWithHint(
		"##ComponentSearch",
		"Search components...",
		&state.component_filter
	);

	ImGui::BeginChild(
		"##ComponentList",
		ImVec2{
			0.0f,
			260.0f
		},
		false
	);

	bool any_visible{ false };

	for (const auto* component :
		 GetRegisteredComponents()) {
		const std::string_view label{
			ComponentDisplayName(
				*component
			)
		};

		if (!ContainsCaseInsensitive(
				label,
				state.component_filter
			) &&
			!ContainsCaseInsensitive(
				component->name,
				state.component_filter
			)) {
			continue;
		}

		any_visible = true;

		const bool is_selected{
			component_name ==
				component->name
		};

		const std::string selectable_label{
			label
		};

		if (ImGui::Selectable(
				selectable_label.c_str(),
				is_selected
			)) {
			component_name =
				component->name;

			changed = true;
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered() &&
			label != component->name) {
			ImGui::SetTooltip(
				"%s",
				component->name.c_str()
			);
		}
	}

	if (!any_visible) {
		ImGui::TextDisabled(
			"No matching components."
		);
	}

	ImGui::EndChild();
	ImGui::EndPopup();

	return changed;
}

[[nodiscard]] bool ComponentQueryConditionMatches(
	Entity entity,
	const ComponentQueryCondition& condition
) {
	if (condition.component.empty()) {
		return false;
	}

	const auto* component{
		ComponentRegistry::Find(
			std::string_view{
				condition.component
			}
		)
	};

	if (!component) {
		return false;
	}

	const bool has_component{
		component->Has(entity)
	};

	return condition.required
		? has_component
		: !has_component;
}

[[nodiscard]] bool ComponentQueryGroupMatches(
	Entity entity,
	const ComponentQueryGroup& group
) {
	if (group.conditions.empty()) {
		return false;
	}

	return std::ranges::all_of(
		group.conditions,
		[entity](
			const ComponentQueryCondition& condition
		) {
			return ComponentQueryConditionMatches(
				entity,
				condition
			);
		}
	);
}

[[nodiscard]] bool ComponentQueryMatches(
	Entity entity,
	const ComponentEntityQuery& query
) {
	if (query.groups.empty()) {
		return false;
	}

	return std::ranges::any_of(
		query.groups,
		[entity](
			const ComponentQueryGroup& group
		) {
			return ComponentQueryGroupMatches(
				entity,
				group
			);
		}
	);
}

[[nodiscard]] std::vector<Entity>
ResolveComponentQuery(
	Scene& scene,
	const ComponentEntityQuery& query
) {
	std::vector<Entity> matches;

	for (Entity entity :
		 scene.Entities()) {
		if (ComponentQueryMatches(
				entity,
				query
			)) {
			matches.push_back(
				entity
			);
		}
	}

	return matches;
}

[[nodiscard]] std::vector<Entity>
ResolveGroupQuery(
	Scene& scene,
	const GroupEntityQuery& query
) {
	std::vector<Entity> matches;

	const auto* group{
		EntityGroupRegistry::Find(
			query.group
		)
	};

	if (!group) {
		return matches;
	}

	for (Entity entity :
		 scene.Entities()) {
		if (std::ranges::contains(
				group->members,
				entity.Get<UUID>()
			)) {
			matches.push_back(
				entity
			);
		}
	}

	return matches;
}

[[nodiscard]] std::vector<Entity>
ResolveRegisteredQuery(
	Scene& scene,
	Entity owner,
	const RegisteredEntityQueryReference& query
) {
	std::vector<Entity> matches;

	const auto* registration{
		EntityQueryRegistry::Find(
			query.key
		)
	};

	if (!registration ||
		!registration->evaluate) {
		return matches;
	}

	for (Entity target :
		 scene.Entities()) {
		if (registration->evaluate(
				EntityQueryContext{
					.scene = scene,
					.owner = owner,
					.target = target,
				}
			)) {
			matches.push_back(
				target
			);
		}
	}

	return matches;
}

[[nodiscard]] std::string ComponentConditionSummary(
	const ComponentQueryCondition& condition
) {
	const auto* component{
		condition.component.empty()
			? nullptr
			: ComponentRegistry::Find(
				  std::string_view{
					  condition.component
				  }
			  )
	};

	const std::string label{
		component
			? std::string{
				  ComponentDisplayName(
					  *component
				  )
			  }
			: condition.component.empty()
				? "<component>"
				: condition.component
	};

	return condition.required
		? "Has " + label
		: "Doesn't Have " + label;
}

[[nodiscard]] std::string ComponentQuerySummary(
	const ComponentEntityQuery& query
) {
	if (query.groups.empty()) {
		return "No component query";
	}

	std::string output;

	for (std::size_t group_index{ 0 };
		 group_index < query.groups.size();
		 ++group_index) {
		if (group_index > 0) {
			output += " OR ";
		}

		const auto& group{
			query.groups[group_index]
		};

		if (query.groups.size() > 1) {
			output += "(";
		}

		if (group.conditions.empty()) {
			output += "<empty>";
		} else {
			for (std::size_t condition_index{ 0 };
				 condition_index <
				 group.conditions.size();
				 ++condition_index) {
				if (condition_index > 0) {
					output += " AND ";
				}

				output +=
					ComponentConditionSummary(
						group.conditions[
							condition_index
						]
					);
			}
		}

		if (query.groups.size() > 1) {
			output += ")";
		}
	}

	return output;
}

[[nodiscard]] std::string TargetSummary(
	Scene& scene,
	Entity owner,
	const EntityTarget& target
) {
	switch (target.type) {
		case EntityTargetType::Entity: {
			Entity entity{
				ResolveEntity(
					scene,
					target.entity
				)
			};

			if (!entity && owner) {
				return EntityDisplayName(owner);
			}

			return entity
				? EntityDisplayName(entity)
				: "Select Entity";
		}

		case EntityTargetType::Components: {
			const std::string summary{
				ComponentQuerySummary(
					target.components
				)
			};

			if (summary.size() <= 52) {
				return summary;
			}

			std::size_t condition_count{ 0 };

			for (const auto& group :
				 target.components.groups) {
				condition_count +=
					group.conditions.size();
			}

			return std::to_string(
					   condition_count
				   ) +
				   (
					   condition_count == 1
						   ? " component condition"
						   : " component conditions"
				   );
		}

		case EntityTargetType::Group: {
			const auto* group{
				EntityGroupRegistry::Find(
					target.group.group
				)
			};

			return group
				? "Group: " +
					  group->label
				: "Select Group";
		}

		case EntityTargetType::Query: {
			const auto* query{
				EntityQueryRegistry::Find(
					target.query.key
				)
			};

			return query
				? query->label
				: "Select Query";
		}
	}

	return "Target";
}

void DrawMatchPreview(
	const std::vector<Entity>& matches
) {
	const std::string label{
		std::to_string(matches.size()) +
		(matches.size() == 1
			? " match"
			: " matches")
	};

	ImGuiTreeNodeFlags flags{
		ImGuiTreeNodeFlags_SpanAvailWidth
	};

	if (matches.empty()) {
		flags |=
			ImGuiTreeNodeFlags_Leaf |
			ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	const bool open{
		ImGui::TreeNodeEx(
			"##Matches",
			flags,
			"%s",
			label.c_str()
		)
	};

	if (matches.empty() || !open) {
		return;
	}

	for (Entity entity : matches) {
		const std::string row{
			EntityDisplayName(entity) +
			" [" +
			UUIDDisplayName(entity) +
			"]"
		};

		ImGui::BulletText(
			"%s",
			row.c_str()
		);
	}

	ImGui::TreePop();
}

bool DrawConditionType(
	ComponentQueryCondition& condition
) {
	const char* preview{
		condition.required
			? "Has"
			: "Doesn't Have"
	};

	bool changed{ false };

	ImGui::SetNextItemWidth(
		-FLT_MIN
	);

	if (ImGui::BeginCombo(
			"##ConditionType",
			preview
		)) {
		if (ImGui::Selectable(
				"Has",
				condition.required
			)) {
			condition.required = true;
			changed = true;
		}

		if (ImGui::Selectable(
				"Doesn't Have",
				!condition.required
			)) {
			condition.required = false;
			changed = true;
		}

		ImGui::EndCombo();
	}

	return changed;
}

bool DrawComponentQueryEditor(
	Scene& scene,
	ComponentEntityQuery& query,
	DemoState& state
) {
	bool changed{ false };
	std::optional<std::size_t> group_to_remove;

	for (std::size_t group_index{ 0 };
		 group_index < query.groups.size();
		 ++group_index) {
		auto& group{
			query.groups[group_index]
		};

		if (group_index > 0) {
			ImGui::TextDisabled("OR");
		}

		ImGui::PushID(
			static_cast<int>(
				group_index
			)
		);

		std::optional<std::size_t> condition_to_remove;

		ImGui::PushStyleVar(
			ImGuiStyleVar_CellPadding,
			ImVec2{
				ImGui::GetStyle().ItemSpacing.x * 0.5f,
				0.0f
			}
		);

		if (ImGui::BeginTable(
				"##ComponentQueryGroup",
				4,
				ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn(
				"Join",
				ImGuiTableColumnFlags_WidthFixed,
				34.0f
			);

			ImGui::TableSetupColumn(
				"Type",
				ImGuiTableColumnFlags_WidthFixed,
				118.0f
			);

			ImGui::TableSetupColumn(
				"Component",
				ImGuiTableColumnFlags_WidthStretch
			);

			ImGui::TableSetupColumn(
				"Remove",
				ImGuiTableColumnFlags_WidthFixed,
				ImGui::GetFrameHeight()
			);

			for (std::size_t condition_index{ 0 };
				 condition_index < group.conditions.size();
				 ++condition_index) {
				auto& condition{
					group.conditions[
						condition_index
					]
				};

				ImGui::PushID(
					static_cast<int>(
						condition_index
					)
				);

				ImGui::TableNextRow(
					0,
					ImGui::GetFrameHeight()
				);

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();

				if (condition_index > 0) {
					ImGui::TextDisabled("AND");
				}

				ImGui::TableSetColumnIndex(1);

				changed |=
					DrawConditionType(
						condition
					);

				ImGui::TableSetColumnIndex(2);

				changed |=
					DrawComponentPicker(
						"##ComponentPicker",
						condition.component,
						state
					);

				ImGui::TableSetColumnIndex(3);

				if (ImGui::Button(
						"X",
						ImVec2{
							ImGui::GetFrameHeight(),
							ImGui::GetFrameHeight()
						}
					)) {
					if (group.conditions.size() == 1) {
						group_to_remove = group_index;
					} else {
						condition_to_remove =
							condition_index;
					}
				}

				ImGui::PopID();

				if (group_to_remove.has_value() ||
					condition_to_remove.has_value()) {
					break;
				}
			}

			ImGui::EndTable();
		}

		ImGui::PopStyleVar();

		if (group_to_remove.has_value()) {
			ImGui::PopID();
			break;
		}

		if (condition_to_remove.has_value()) {
			group.conditions.erase(
				group.conditions.begin() +
				static_cast<std::ptrdiff_t>(
					condition_to_remove.value()
				)
			);

			changed = true;
		}

		const float row_height{
			ImGui::GetFrameHeight()
		};

		if (ImGui::Button(
				"+ AND",
				ImVec2{
					0.0f,
					row_height
				}
			)) {
			group.conditions.push_back(
				ComponentQueryCondition{}
			);

			changed = true;
		}

		if (query.groups.size() > 1) {
			ImGui::SameLine(
				0.0f,
				ImGui::GetStyle().ItemSpacing.x
			);

			if (ImGui::Button(
					"Remove Group",
					ImVec2{
						0.0f,
						row_height
					}
				)) {
				group_to_remove =
					group_index;
			}
		}

		ImGui::PopID();

		if (group_to_remove.has_value()) {
			break;
		}
	}

	if (group_to_remove.has_value()) {
		query.groups.erase(
			query.groups.begin() +
			static_cast<std::ptrdiff_t>(
				group_to_remove.value()
			)
		);

		changed = true;
	}

	if (ImGui::Button(
			"+ OR Group",
			ImVec2{
				0.0f,
				ImGui::GetFrameHeight()
			}
		)) {
		query.groups.push_back(
			ComponentQueryGroup{
				.conditions{
					ComponentQueryCondition{},
				},
			}
		);

		changed = true;
	}

	DrawMatchPreview(
		ResolveComponentQuery(
			scene,
			query
		)
	);

	return changed;
}

bool DrawGroupPicker(
	GroupEntityQuery& query,
	DemoState& state
) {
	const auto* selected{
		EntityGroupRegistry::Find(
			query.group
		)
	};

	const std::string preview{
		selected
			? selected->label
			: "Select Group"
	};

	bool changed{ false };

	ImGui::SetNextItemWidth(
		280.0f
	);

	if (!ImGui::BeginCombo(
			"##GroupPicker",
			preview.c_str()
		)) {
		return false;
	}

	ImGui::SetNextItemWidth(
		-FLT_MIN
	);

	ImGui::InputTextWithHint(
		"##GroupSearch",
		"Search groups...",
		&state.group_filter
	);

	auto groups{
		EntityGroupRegistry::Groups()
	};

	std::ranges::sort(
		groups,
		{},
		&RegisteredEntityGroup::label
	);

	bool any_visible{ false };

	for (const auto& group : groups) {
		if (!ContainsCaseInsensitive(
				group.label,
				state.group_filter
			) &&
			!ContainsCaseInsensitive(
				group.key,
				state.group_filter
			)) {
			continue;
		}

		any_visible = true;

		if (ImGui::Selectable(
				group.label.c_str(),
				query.group == group.key
			)) {
			query.group = group.key;
			changed = true;
		}
	}

	if (!any_visible) {
		ImGui::TextDisabled(
			"No matching groups."
		);
	}

	ImGui::EndCombo();
	return changed;
}

bool DrawGroupQueryEditor(
	Scene& scene,
	GroupEntityQuery& query,
	DemoState& state
) {
	bool changed{
		DrawGroupPicker(
			query,
			state
		)
	};

	const auto* selected{
		EntityGroupRegistry::Find(
			query.group
		)
	};

	if (selected) {
		ImGui::SameLine();

		ImGui::TextDisabled(
			"%zu %s",
			selected->members.size(),
			selected->members.size() == 1
				? "member"
				: "members"
		);
	}

	DrawMatchPreview(
		ResolveGroupQuery(
			scene,
			query
		)
	);

	return changed;
}

bool DrawRegisteredQueryPicker(
	RegisteredEntityQueryReference& query,
	DemoState& state
) {
	const auto* selected{
		EntityQueryRegistry::Find(
			query.key
		)
	};

	const std::string preview{
		selected
			? selected->label
			: "Select Query"
	};

	bool changed{ false };

	ImGui::SetNextItemWidth(
		320.0f
	);

	if (!ImGui::BeginCombo(
			"##QueryPicker",
			preview.c_str()
		)) {
		return false;
	}

	ImGui::SetNextItemWidth(
		-FLT_MIN
	);

	ImGui::InputTextWithHint(
		"##QuerySearch",
		"Search queries...",
		&state.query_filter
	);

	auto queries{
		EntityQueryRegistry::Queries()
	};

	std::ranges::sort(
		queries,
		[](const auto& lhs, const auto& rhs) {
			if (lhs.group != rhs.group) {
				return lhs.group < rhs.group;
			}

			return lhs.label < rhs.label;
		}
	);

	std::vector<std::string> visible_groups;
	bool has_ungrouped{ false };

	auto visible = [&](const RegisteredEntityQuery& registration) {
		return
			ContainsCaseInsensitive(
				registration.label,
				state.query_filter
			) ||
			ContainsCaseInsensitive(
				registration.group,
				state.query_filter
			) ||
			ContainsCaseInsensitive(
				registration.description,
				state.query_filter
			);
	};

	for (const auto& registration : queries) {
		if (!visible(registration)) {
			continue;
		}

		if (registration.group.empty()) {
			has_ungrouped = true;
			continue;
		}

		if (!std::ranges::contains(
				visible_groups,
				registration.group
			)) {
			visible_groups.push_back(
				registration.group
			);
		}
	}

	bool any_visible{
		has_ungrouped ||
		!visible_groups.empty()
	};

	for (const auto& registration : queries) {
		if (!registration.group.empty() ||
			!visible(registration)) {
			continue;
		}

		if (ImGui::Selectable(
				registration.label.c_str(),
				query.key == registration.key
			)) {
			query.key = registration.key;
			changed = true;
		}

		if (ImGui::IsItemHovered() &&
			!registration.description.empty()) {
			ImGui::SetTooltip(
				"%s",
				registration.description.c_str()
			);
		}
	}

	for (const auto& group : visible_groups) {
		if (!ImGui::BeginMenu(
				group.c_str()
			)) {
			continue;
		}

		for (const auto& registration : queries) {
			if (registration.group != group ||
				!visible(registration)) {
				continue;
			}

			if (ImGui::MenuItem(
					registration.label.c_str(),
					nullptr,
					query.key == registration.key
				)) {
				query.key = registration.key;
				changed = true;
			}

			if (ImGui::IsItemHovered() &&
				!registration.description.empty()) {
				ImGui::SetTooltip(
					"%s",
					registration.description.c_str()
				);
			}
		}

		ImGui::EndMenu();
	}

	if (!any_visible) {
		ImGui::TextDisabled(
			"No matching queries."
		);
	}

	ImGui::EndCombo();
	return changed;
}

bool DrawRegisteredQueryEditor(
	Scene& scene,
	Entity owner,
	RegisteredEntityQueryReference& query,
	DemoState& state
) {
	bool changed{
		DrawRegisteredQueryPicker(
			query,
			state
		)
	};

	const auto* selected{
		EntityQueryRegistry::Find(
			query.key
		)
	};

	if (selected &&
		!selected->description.empty()) {
		ImGui::TextDisabled(
			"%s",
			selected->description.c_str()
		);
	}

	DrawMatchPreview(
		ResolveRegisteredQuery(
			scene,
			owner,
			query
		)
	);

	return changed;
}

[[nodiscard]] float TargetPopupMinWidth(
	EntityTargetType type
) {
	switch (type) {
		case EntityTargetType::Entity:
			return kEntityModeMinWidth;

		case EntityTargetType::Components:
			return kComponentsModeMinWidth;

		case EntityTargetType::Group:
			return kGroupsModeMinWidth;

		case EntityTargetType::Query:
			return kQueriesModeMinWidth;
	}

	return kEntityModeMinWidth;
}

bool DrawEntityTargetEditor(
	Scene& scene,
	Entity owner,
	EntityTarget& target,
	DemoState& state
) {
	bool changed{ false };

	const float close_width{
		ImGui::GetFrameHeight()
	};

	const float spacing{
		ImGui::GetStyle()
			.ItemSpacing.x
	};

	const float available{
		ImGui::GetContentRegionAvail().x
	};

	const float mode_width{
		std::max(
			70.0f,
			(
				available -
				close_width -
				spacing * 4.0f
			) /
			4.0f
		)
	};

	if (ImGui::Selectable(
			"Entity",
			target.type ==
				EntityTargetType::Entity,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type =
			EntityTargetType::Entity;

		if (owner) {
			SetEntityReference(
				target.entity,
				owner
			);
		}

		changed = true;
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			"Components",
			target.type ==
				EntityTargetType::Components,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type =
			EntityTargetType::Components;

		changed = true;
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			"Groups",
			target.type ==
				EntityTargetType::Group,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type =
			EntityTargetType::Group;

		changed = true;
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			"Queries",
			target.type ==
				EntityTargetType::Query,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type =
			EntityTargetType::Query;

		changed = true;
	}

	ImGui::SameLine();

	if (ImGui::Button(
			"X",
			ImVec2{
				close_width,
				close_width
			}
		)) {
		ImGui::CloseCurrentPopup();
		return changed;
	}

	ImGui::Separator();

	switch (target.type) {
		case EntityTargetType::Entity:
			DrawMiniHierarchy(
				scene,
				owner,
				target.entity,
				state
			);
			break;

		case EntityTargetType::Components:
			changed |=
				DrawComponentQueryEditor(
					scene,
					target.components,
					state
				);
			break;

		case EntityTargetType::Group:
			changed |=
				DrawGroupQueryEditor(
					scene,
					target.group,
					state
				);
			break;

		case EntityTargetType::Query:
			changed |=
				DrawRegisteredQueryEditor(
					scene,
					owner,
					target.query,
					state
				);
			break;
	}

	return changed;
}

bool DrawTargetButton(
	Scene& scene,
	Entity owner,
	EntityTarget& target,
	DemoState& state
) {
	const std::string summary{
		TargetSummary(
			scene,
			owner,
			target
		)
	};

	bool changed{ false };

	if (ImGui::Button(
			summary.c_str(),
			ImVec2{
				ImGui::GetContentRegionAvail().x,
				0.0f
			}
		)) {
		if (target.type ==
				EntityTargetType::Entity &&
			!ResolveEntity(
				scene,
				target.entity
			) &&
			owner) {
			SetEntityReference(
				target.entity,
				owner
			);
		}

		ImGui::OpenPopup(
			"##TargetPopup"
		);
	}

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{
			TargetPopupMinWidth(
				target.type
			),
			0.0f
		},
		ImVec2{
			kTargetPopupMaxWidth,
			FLT_MAX
		}
	);

	if (ImGui::BeginPopup(
			"##TargetPopup"
		)) {
		changed |=
			DrawEntityTargetEditor(
				scene,
				owner,
				target,
				state
			);

		ImGui::EndPopup();
	}

	return changed;
}

void DrawFakeMoveToAction(
	Scene& scene,
	Entity owner,
	EntityTarget& target,
	DemoState& state
) {
	static V2_float destination{
		100.0f,
		50.0f
	};

	static bool relative{ true };
	static float duration_ms{ 300.0f };

	ImGui::PushID(
		"MoveToDemo"
	);

	const bool open{
		ImGui::TreeNodeEx(
			"Move To",
			ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (open) {
		if (ImGui::BeginTable(
				"##MoveToProperties",
				2,
				ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn(
				"Label",
				ImGuiTableColumnFlags_WidthFixed,
				90.0f
			);

			ImGui::TableSetupColumn(
				"Value",
				ImGuiTableColumnFlags_WidthStretch
			);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Target");
			ImGui::TableSetColumnIndex(1);

			DrawTargetButton(
				scene,
				owner,
				target,
				state
			);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(
				"Destination"
			);

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(
				-FLT_MIN
			);

			ImGui::DragFloat2(
				"##Destination",
				&destination.x,
				0.1f
			);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(
				"Relative"
			);

			ImGui::TableSetColumnIndex(1);

			ImGui::Checkbox(
				"##Relative",
				&relative
			);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(
				"Duration"
			);

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(
				-FLT_MIN
			);

			ImGui::DragFloat(
				"##Duration",
				&duration_ms,
				1.0f,
				0.0f,
				10000.0f,
				"%.0f ms"
			);

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void EnsureOwner(
	EditorContext& ctx,
	Scene& scene,
	DemoState& state
) {
	if (ResolveEntity(
			scene,
			state.owner
		)) {
		return;
	}

	Entity selected{
		ctx.editor
			.GetSceneHierarchyPanel()
			.GetSelectedEntity()
	};

	if (selected &&
		scene.Entities().Contains(
			selected
		)) {
		SetEntityReference(
			state.owner,
			selected
		);

		return;
	}

	for (Entity entity : scene.Entities()) {
		SetEntityReference(
			state.owner,
			entity
		);

		return;
	}

	state.owner = {};
}

void EnsureTargetDefaults(
	Entity owner,
	EntityTarget& target,
	DemoState& state
) {
	if (target.type ==
			EntityTargetType::Entity &&
		!target.entity.uuid.has_value() &&
		owner) {
		SetEntityReference(
			target.entity,
			owner
		);
	}

	if (!state.components_initialized) {
		state.components_initialized = true;

		if (target.components.groups.empty()) {
			target.components.groups.push_back(
				ComponentQueryGroup{
					.conditions{
						ComponentQueryCondition{},
					},
				}
			);
		}
	}
}

} // namespace

void DrawScriptTargetingDemo(
	EditorContext& ctx
) {
	static DemoState state;

	EnsureDemoGroupsRegistered();

	if (!ImGui::Begin(
			"Script Targeting Demo"
		)) {
		ImGui::End();
		return;
	}

	Scene* scene{
		ctx.editor
			.GetSceneListPanel()
			.GetSelectedScene()
	};

	if (!scene) {
		ImGui::TextDisabled(
			"No scene selected."
		);

		ImGui::End();
		return;
	}

	EnsureOwner(
		ctx,
		*scene,
		state
	);

	Entity owner{
		ResolveEntity(
			*scene,
			state.owner
		)
	};

	EnsureTargetDefaults(
		owner,
		state.target,
		state
	);

	Entity hierarchy_selection{
		ctx.editor
			.GetSceneHierarchyPanel()
			.GetSelectedEntity()
	};

	ImGui::TextUnformatted(
		"Script Owner:"
	);

	ImGui::SameLine();

	if (owner) {
		ImGui::TextUnformatted(
			EntityDisplayName(
				owner
			).c_str()
		);
	} else {
		ImGui::TextDisabled(
			"None"
		);
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(
		!hierarchy_selection ||
		!scene->Entities().Contains(
			hierarchy_selection
		)
	);

	if (ImGui::SmallButton(
			"Use Selected"
		)) {
		SetEntityReference(
			state.owner,
			hierarchy_selection
		);

		owner =
			hierarchy_selection;

		if (state.target.type ==
			EntityTargetType::Entity) {
			SetEntityReference(
				state.target.entity,
				owner
			);
		}
	}

	ImGui::EndDisabled();

	DrawFakeMoveToAction(
		*scene,
		owner,
		state.target,
		state
	);

	ImGui::End();
}

} // namespace ptgn::editor::entity_target_demo
