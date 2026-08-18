#include "panels/entity_filter_editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_group.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_query.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "serialization/json/json.h"

namespace ptgn::editor::inspector {

using EntityReference = ::ptgn::EntityReference;
using ComponentQueryCondition = ::ptgn::ComponentQueryCondition;
using ComponentQueryGroup = ::ptgn::ComponentQueryGroup;
using ComponentEntityQuery = ::ptgn::ComponentEntityQuery;
using GroupEntityQuery = ::ptgn::GroupEntityQuery;
using RegisteredEntityQueryReference = ::ptgn::RegisteredEntityQueryReference;
using EntityQueryContext = ::ptgn::EntityQueryContext;
using RegisteredEntityQuery = ::ptgn::RegisteredEntityQuery;
using EntityQueryRegistry = ::ptgn::EntityQueryRegistry;
using RegisteredEntityGroup = ::ptgn::RegisteredEntityGroup;
using EntityGroupRegistry = ::ptgn::EntityGroupRegistry;

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

void SetEntityRef(
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
			"##EntityFilter",
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
	EntityFilterEditorState& state
) {
	if (owner) {
		if (ImGui::Button(
				"Select Owner"
			)) {
			SetEntityRef(
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
		SetEntityRef(
			reference,
			picked_entity
		);
	}

	ImGui::EndChild();
}

bool DrawComponentPicker(
	const char* id,
	std::string& component_name,
	EntityFilterEditorState& state
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

[[nodiscard]] std::string FilterSummary(
	Scene* scene,
	[[maybe_unused]] Entity owner,
	const EntityFilter& target
) {
	switch (target.type) {
		case EntityFilterType::Any:
			return "Any Entity";

		case EntityFilterType::Entity: {
			if (!scene) {
				return target.entity.uuid.has_value()
					? (target.entity.tag.empty() ? "Selected Entity" : target.entity.tag)
					: "Select Entity";
			}

			Entity entity{ ResolveEntity(*scene, target.entity) };
			return entity ? EntityDisplayName(entity) : "Select Entity";
		}

		case EntityFilterType::Components: {
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

		case EntityFilterType::Group: {
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

		case EntityFilterType::Query: {
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
	Scene* scene,
	ComponentEntityQuery& query,
	EntityFilterEditorState& state
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

	if (scene) {
		DrawMatchPreview(ResolveComponentQuery(*scene, query));
	}

	return changed;
}

bool DrawGroupPicker(
	GroupEntityQuery& query,
	EntityFilterEditorState& state
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
	Scene* scene,
	GroupEntityQuery& query,
	EntityFilterEditorState& state
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

	if (scene) {
		DrawMatchPreview(ResolveGroupQuery(*scene, query));
	}

	return changed;
}

bool DrawRegisteredQueryPicker(
	RegisteredEntityQueryReference& query,
	EntityFilterEditorState& state
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
	Scene* scene,
	Entity owner,
	RegisteredEntityQueryReference& query,
	EntityFilterEditorState& state
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

	if (scene) {
		DrawMatchPreview(ResolveRegisteredQuery(*scene, owner, query));
	}

	return changed;
}

[[nodiscard]] float FilterPopupMinWidth(
	EntityFilterType type
) {
	switch (type) {
		case EntityFilterType::Any:
			return 300.0f;

		case EntityFilterType::Entity:
			return kEntityModeMinWidth;

		case EntityFilterType::Components:
			return kComponentsModeMinWidth;

		case EntityFilterType::Group:
			return kGroupsModeMinWidth;

		case EntityFilterType::Query:
			return kQueriesModeMinWidth;
	}

	return kEntityModeMinWidth;
}

bool DrawEntityFilterEditor(
	Scene* scene,
	Entity owner,
	EntityFilter& target,
	EntityFilterEditorState& state
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
			64.0f,
			(available - close_width - spacing * 5.0f) / 5.0f
		)
	};

	if (ImGui::Selectable(
			"Any",
			target.type == EntityFilterType::Any,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{ mode_width, close_width }
		)) {
		target.type = EntityFilterType::Any;
		changed = true;
	}

	ImGui::SameLine();

	{
		if (!scene) {
			ImGui::BeginDisabled();
		}

		if (ImGui::Selectable(
				"Entity",
				target.type == EntityFilterType::Entity,
				ImGuiSelectableFlags_DontClosePopups,
				ImVec2{ mode_width, close_width }
			)) {
			target.type = EntityFilterType::Entity;
			changed = true;
		}

		if (!scene) {
			ImGui::EndDisabled();
		}
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			"Components",
			target.type ==
				EntityFilterType::Components,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type = EntityFilterType::Components;
		if (target.components.groups.empty()) {
			target.components.groups.push_back(ComponentQueryGroup{
				.conditions{ ComponentQueryCondition{} },
			});
		}

		changed = true;
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			"Groups",
			target.type ==
				EntityFilterType::Group,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type =
			EntityFilterType::Group;

		changed = true;
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			"Queries",
			target.type ==
				EntityFilterType::Query,
			ImGuiSelectableFlags_DontClosePopups,
			ImVec2{
				mode_width,
				close_width
			}
		)) {
		target.type =
			EntityFilterType::Query;

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
		case EntityFilterType::Any:
			ImGui::TextDisabled("All entities match this filter.");
			break;

		case EntityFilterType::Entity:
			if (scene) {
				DrawMiniHierarchy(*scene, owner, target.entity, state);
			} else {
				ImGui::TextDisabled("Exact entity selection requires a scene instance.");
			}
			break;

		case EntityFilterType::Components:
			changed |=
				DrawComponentQueryEditor(
					scene,
					target.components,
					state
				);
			break;

		case EntityFilterType::Group:
			changed |=
				DrawGroupQueryEditor(
					scene,
					target.group,
					state
				);
			break;

		case EntityFilterType::Query:
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

bool DrawEntityFilterButtonImpl(
	Scene* scene,
	Entity owner,
	EntityFilter& target,
	EntityFilterEditorState& state
) {
	ImGui::PushID(std::addressof(target));

	const std::string summary{
		FilterSummary(
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
		ImGui::OpenPopup(
			"##EntityFilterPopup"
		);
	}

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{
			FilterPopupMinWidth(
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
			"##EntityFilterPopup"
		)) {
		changed |=
			DrawEntityFilterEditor(
				scene,
				owner,
				target,
				state
			);

		ImGui::EndPopup();
	}

	ImGui::PopID();
	return changed;
}

} // namespace

bool DrawEntityFilterButton(
	Scene* scene, Entity owner, EntityFilter& filter, EntityFilterEditorState& state
) {
	return DrawEntityFilterButtonImpl(scene, owner, filter, state);
}

} // namespace ptgn::editor::inspector
