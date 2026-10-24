#include "panels/entity_filter_editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/util/string.h"
#include "panels/entity_hierarchy.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_group.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_query.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interactive.h"
#include "runtime/scene/scene.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/slider.h"
#include "serialization/json/json.h"

namespace ptgn::editor::inspector {

using EntityReference				 = ::ptgn::EntityReference;
using ComponentQueryCondition		 = ::ptgn::ComponentQueryCondition;
using ComponentQueryGroup			 = ::ptgn::ComponentQueryGroup;
using ComponentEntityQuery			 = ::ptgn::ComponentEntityQuery;
using TagEntityQuery				 = ::ptgn::TagEntityQuery;
using RegisteredEntityQueryReference = ::ptgn::RegisteredEntityQueryReference;
using EntityQueryContext			 = ::ptgn::EntityQueryContext;
using RegisteredEntityQuery			 = ::ptgn::RegisteredEntityQuery;
using EntityQueryRegistry			 = ::ptgn::EntityQueryRegistry;

namespace {

constexpr std::size_t kMaxHierarchyDepth{ 256 };
constexpr float kEntityHierarchyHeight{ 300.0f };
constexpr float kEntityModeMinWidth{ 420.0f };
constexpr float kComponentsModeMinWidth{ 720.0f };
constexpr float kGroupsModeMinWidth{ 360.0f };
constexpr float kQueriesModeMinWidth{ 400.0f };
constexpr float kTargetPopupWidth{ kComponentsModeMinWidth };
constexpr float kTargetPopupScreenMargin{ 8.0f };
constexpr float kComponentRowExtraHeight{ 4.0f };

[[nodiscard]] std::string_view ComponentDisplayName(const RegisteredComponent& component) {
	std::string_view name{ component.name };

	if (auto separator{ name.rfind("::") }; separator != std::string_view::npos) {
		name.remove_prefix(separator + 2);
	}

	return name;
}

[[nodiscard]] std::string EntityDisplayName(Entity entity) {
	if (!entity) {
		return "Missing Entity";
	}

	const auto& tag{ entity.Get<Tag>().value };

	return tag.empty() ? "Entity" : tag;
}

[[nodiscard]] std::string UUIDDisplayName(Entity entity) {
	if (!entity) {
		return {};
	}

	json value = entity.Get<UUID>();

	if (value.is_string()) {
		return value.get<std::string>();
	}

	return value.dump();
}

void SetEntityRef(EntityReference& reference, Entity entity) {
	if (!entity) {
		reference = {};

		return;
	}

	reference.uuid = entity.Get<UUID>();
	reference.tag  = entity.Get<Tag>().value;
}

[[nodiscard]] Entity ResolveEntity(Scene& scene, const EntityReference& reference) {
	if (!reference.uuid.has_value()) {
		return {};
	}

	for (Entity entity : scene.Entities()) {
		if (entity.Get<UUID>() == reference.uuid.value()) {
			return entity;
		}
	}

	return {};
}

[[nodiscard]] std::vector<const RegisteredComponent*> GetRegisteredComponents() {
	std::vector<const RegisteredComponent*> components;

	for (const auto& component : ComponentRegistry::Components()) {
		components.push_back(std::addressof(component));
	}

	std::ranges::sort(components, [](const auto* lhs, const auto* rhs) {
		return ComponentDisplayName(*lhs) < ComponentDisplayName(*rhs);
	});

	return components;
}

[[nodiscard]] bool IsHierarchyEntityExcluded(
	Entity entity, Entity excluded_entity, std::span<const Entity> excluded_entities
) {
	return entity == excluded_entity || std::ranges::contains(excluded_entities, entity);
}

[[nodiscard]] bool IsManagedUiPart(Entity entity) {
	if (!entity) {
		return false;
	}

	// Use the same managed UI part rules as the Scene Hierarchy.
	if (entity.Has<::ptgn::impl::InteractiveTag>()) {
		return true;
	}
	if (!HasParent(entity)) {
		return false;
	}

	Entity parent{ GetParent(entity) };
	if (entity.HasAny<
			ButtonBackgroundVisuals, ButtonBorderVisuals, ButtonSpriteVisuals,
			ButtonTextVisuals>() &&
		parent.Has<::ptgn::impl::ButtonData>()) {
		return true;
	}
	if (entity.HasAny<
			::ptgn::impl::SliderThumbData, ::ptgn::impl::SliderTrackData,
			::ptgn::impl::SliderValueTextData>() &&
		parent.Has<::ptgn::impl::SliderData>()) {
		return true;
	}
	if (entity.HasAny<
			::ptgn::impl::SliderTrackBackgroundData, ::ptgn::impl::SliderTrackBorderData,
			::ptgn::impl::SliderTrackSpriteData>() &&
		parent.Has<::ptgn::impl::SliderTrackData>()) {
		return true;
	}
	return entity.Has<::ptgn::impl::DialoguePart>() && parent.Has<::ptgn::impl::DialogueData>();
}

[[nodiscard]] bool IsOwnerDescendant(Entity entity, Entity owner) {
	for (std::size_t depth{ 0 }; entity && depth < kMaxHierarchyDepth; ++depth) {
		if (!HasParent(entity)) {
			break;
		}
		entity = GetParent(entity);
		if (entity == owner) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool HideEntityBranch(
	Entity entity, Entity owner, Entity excluded_entity, std::span<const Entity> excluded_entities,
	const EntityFilterEditorOptions& options
) {
	return (options.hide_managed_ui_parts && IsManagedUiPart(entity)) ||
		   (options.hide_owner_descendants && owner && IsOwnerDescendant(entity, owner)) ||
		   (options.hide_excluded_descendants &&
			IsHierarchyEntityExcluded(entity, excluded_entity, excluded_entities));
}

[[nodiscard]] bool EntityOrDescendantMatchesFilter(
	Entity entity, std::string_view filter_text, Entity owner, Entity excluded_entity,
	std::span<const Entity> excluded_entities, const EntityFilterEditorOptions& options,
	std::size_t depth = 0
) {
	if (!entity || depth >= kMaxHierarchyDepth ||
		HideEntityBranch(entity, owner, excluded_entity, excluded_entities, options)) {
		return false;
	}

	if (!IsHierarchyEntityExcluded(entity, excluded_entity, excluded_entities) &&
		hierarchy::MatchesFilter(entity, filter_text)) {
		return true;
	}

	if (HasChildren(entity)) {
		for (Entity child : GetChildren(entity)) {
			if (EntityOrDescendantMatchesFilter(
					child, filter_text, owner, excluded_entity, excluded_entities, options,
					depth + 1
				)) {
				return true;
			}
		}
	}
	return false;
}

void DrawEntityHierarchyNode(
	Entity entity, const EntityReference* current, std::string_view filter, Entity* picked_entity,
	Entity owner, Entity excluded_entity, std::span<const Entity> excluded_entities,
	const EntityFilterEditorOptions& options, std::size_t depth = 0
) {
	if (!entity || depth >= kMaxHierarchyDepth ||
		HideEntityBranch(entity, owner, excluded_entity, excluded_entities, options)) {
		return;
	}

	if (IsHierarchyEntityExcluded(entity, excluded_entity, excluded_entities)) {
		if (HasChildren(entity)) {
			auto children{ GetChildren(entity) };
			SortByLocalDepth(children);

			for (Entity child : children) {
				DrawEntityHierarchyNode(
					child, current, filter, picked_entity, owner, excluded_entity,
					excluded_entities, options, depth + 1
				);
			}
		}

		return;
	}

	if (!EntityOrDescendantMatchesFilter(
			entity, filter, owner, excluded_entity, excluded_entities, options, depth
		)) {
		return;
	}

	std::vector<Entity> children;
	auto collect_children = [&](auto&& self, Entity parent) -> void {
		if (!HasChildren(parent)) {
			return;
		}

		auto direct_children{ GetChildren(parent) };
		SortByLocalDepth(direct_children);

		for (Entity child : direct_children) {
			if (HideEntityBranch(child, owner, excluded_entity, excluded_entities, options)) {
				continue;
			}
			if (IsHierarchyEntityExcluded(child, excluded_entity, excluded_entities)) {
				self(self, child);
				continue;
			}

			if (EntityOrDescendantMatchesFilter(
					child, filter, owner, excluded_entity, excluded_entities, options, depth + 1
				)) {
				children.emplace_back(child);
			}
		}
	};
	collect_children(collect_children, entity);
	bool has_visible_children{ !children.empty() };
	bool selected{ current && current->uuid.has_value() &&
				   entity.Get<UUID>() == current->uuid.value() };
	ImGui::PushID(entity.Get<UUID>());
	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_OpenOnArrow |
							  ImGuiTreeNodeFlags_OpenOnDoubleClick |
							  ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen };
	if (selected) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (!has_visible_children) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	} else if (!filter.empty()) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	std::string label{ EntityDisplayName(entity) };
	bool open{ ImGui::TreeNodeEx("##EntityFilter", flags, "%s", label.c_str()) };

	if (picked_entity && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		*picked_entity = entity;
	}

	if (ImGui::IsItemHovered()) {
		std::string uuid{ UUIDDisplayName(entity) };
		ImGui::SetTooltip("uuid: %s", uuid.c_str());
	}

	if (has_visible_children && open) {
		for (Entity child : children) {
			DrawEntityHierarchyNode(
				child, current, filter, picked_entity, owner, excluded_entity, excluded_entities,
				options, depth + 1
			);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

bool DrawMiniHierarchy(
	Scene& scene, Entity owner, EntityReference* entity_reference, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options
) {
	bool changed{ false };
	bool allow_select_owner{ options.allow_select_owner };
	Entity excluded_entity{ options.exclude_owner ? owner : Entity{} };
	std::span<const Entity> excluded_entities{ options.excluded_entities };

	Entity selected{ ResolveEntity(scene, *entity_reference) };

	if (selected) {
		std::string selected_label{ "Selected Entity: " + EntityDisplayName(selected) +
									" [uuid: " + UUIDDisplayName(selected) + "]" };
		ImGui::TextUnformatted(selected_label.c_str());
	} else {
		ImGui::TextUnformatted("Selected Entity: None");
	}

	if (allow_select_owner && owner && ImGui::Button("Select Owner")) {
		SetEntityRef(*entity_reference, owner);
		changed = true;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##HierarchyFilter", "Filter: player, -enemy, *hidden, *shown", &state.hierarchy_filter
	);
	hierarchy::DrawFilterTooltip();

	ImGui::BeginChild(
		"##EntityHierarchy", ImVec2{ 0.0f, kEntityHierarchyHeight }, ImGuiChildFlags_Borders
	);

	Entity picked_entity;

	std::vector<Entity> roots;
	for (Entity entity : scene.Entities()) {
		if (entity && !HasParent(entity)) {
			roots.push_back(entity);
		}
	}
	SortByLocalDepth(roots);
	for (Entity root : roots) {
		DrawEntityHierarchyNode(
			root, entity_reference, state.hierarchy_filter, std::addressof(picked_entity), owner,
			excluded_entity, excluded_entities, options
		);
	}

	if (picked_entity) {
		SetEntityRef(*entity_reference, picked_entity);
		changed = true;
	}

	ImGui::EndChild();

	return changed;
}

[[nodiscard]] bool IsOwnerReference(Entity owner, const EntityReference& reference) {
	return owner && reference.uuid.has_value() && owner.Get<UUID>() == reference.uuid.value();
}

[[nodiscard]] float GetPopupWidth(const EntityFilterEditorOptions& options) {
	if (options.single_entity_only) {
		return kEntityModeMinWidth;
	}

	float width{ 0.0f };

	if (options.show_entity || options.single_entity_only) {
		width = std::max(width, kEntityModeMinWidth);
	}

	if (options.show_components) {
		width = std::max(width, kComponentsModeMinWidth);
	}

	if (options.show_tags) {
		width = std::max(width, kGroupsModeMinWidth);
	}

	if (options.show_queries) {
		width = std::max(width, kQueriesModeMinWidth);
	}

	return width > 0.0f ? width : kTargetPopupWidth;
}

bool DrawComponentPicker(
	const char* id, std::string& component_name, EntityFilterEditorState& state
) {
	const auto* selected{ component_name.empty()
							  ? nullptr
							  : ComponentRegistry::Find(std::string_view{ component_name }) };
	std::string preview{ selected				  ? std::string{ ComponentDisplayName(*selected) }
						 : component_name.empty() ? "Select Component"
												  : component_name + " (Missing)" };
	bool changed{ false };

	if (ImGui::Button(
			preview.c_str(), ImVec2{ ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() }
		)) {
		state.component_filter.clear();
		ImGui::OpenPopup(id);
	}

	ImGui::SetNextWindowSizeConstraints(ImVec2{ 320.0f, 0.0f }, ImVec2{ 440.0f, 420.0f });

	if (!ImGui::BeginPopup(id)) {
		return changed;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##ComponentSearch", "Search components...", &state.component_filter);
	ImGui::BeginChild("##ComponentList", ImVec2{ 0.0f, 260.0f }, false);
	bool any_visible{ false };

	for (const auto* component : GetRegisteredComponents()) {
		std::string_view label{ ComponentDisplayName(*component) };

		if (!ContainsCaseInsensitive(label, state.component_filter) &&
			!ContainsCaseInsensitive(component->name, state.component_filter)) {
			continue;
		}

		any_visible = true;
		bool is_selected{ component_name == component->name };
		std::string selectable_label{ label };

		if (ImGui::Selectable(selectable_label.c_str(), is_selected)) {
			component_name = component->name;
			changed		   = true;
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered() && label != component->name) {
			ImGui::SetTooltip("%s", component->name.c_str());
		}
	}

	if (!any_visible) {
		ImGui::TextDisabled("No matching components.");
	}

	ImGui::EndChild();
	ImGui::EndPopup();

	return changed;
}

[[nodiscard]] bool ComponentQueryConditionMatches(
	Entity entity, const ComponentQueryCondition& condition
) {
	if (condition.component.empty()) {
		return false;
	}

	const auto* component{ ComponentRegistry::Find(std::string_view{ condition.component }) };

	if (!component) {
		return false;
	}

	bool has_component{ component->Has(entity) };

	return condition.required ? has_component : !has_component;
}

[[nodiscard]] bool ComponentQueryGroupMatches(Entity entity, const ComponentQueryGroup& group) {
	if (group.conditions.empty()) {
		return false;
	}

	return std::ranges::all_of(
		group.conditions, [entity](const ComponentQueryCondition& condition) {
			return ComponentQueryConditionMatches(entity, condition);
		}
	);
}

[[nodiscard]] bool ComponentQueryMatches(Entity entity, const ComponentEntityQuery& query) {
	if (query.groups.empty()) {
		return false;
	}

	return std::ranges::any_of(query.groups, [entity](const ComponentQueryGroup& group) {
		return ComponentQueryGroupMatches(entity, group);
	});
}

[[nodiscard]] std::vector<Entity> ResolveComponentQuery(
	Scene& scene, const ComponentEntityQuery& query
) {
	std::vector<Entity> matches;

	for (Entity entity : scene.Entities()) {
		if (ComponentQueryMatches(entity, query)) {
			matches.push_back(entity);
		}
	}

	return matches;
}

[[nodiscard]] std::vector<Entity> ResolveTagQuery(Scene& scene, const TagEntityQuery& query) {
	std::vector<Entity> matches;

	for (Entity entity : scene.Entities()) {
		auto* membership{ entity.TryGet<Tags>() };

		if (!membership) {
			continue;
		}

		bool matches_group{ std::ranges::any_of(
			query.tags, [&membership](const std::string& group) {
				return !group.empty() && std::ranges::contains(membership->values, group);
			}
		) };

		if (matches_group) {
			matches.push_back(entity);
		}
	}

	return matches;
}

[[nodiscard]] std::vector<Entity> ResolveRegisteredQuery(
	Scene& scene, Entity owner, const RegisteredEntityQueryReference& query
) {
	std::vector<Entity> matches;
	const auto* registration{ EntityQueryRegistry::Find(query.key) };

	if (!registration || !registration->evaluate) {
		return matches;
	}

	for (Entity target : scene.Entities()) {
		if (registration->evaluate(
				EntityQueryContext{
					.scene	= scene,
					.owner	= owner,
					.target = target,
				}
			)) {
			matches.push_back(target);
		}
	}

	return matches;
}

[[nodiscard]] std::string ComponentConditionSummary(const ComponentQueryCondition& condition) {
	const auto* component{ condition.component.empty()
							   ? nullptr
							   : ComponentRegistry::Find(std::string_view{ condition.component }) };
	std::string label{ component ? std::string{ ComponentDisplayName(*component) }
					   : condition.component.empty() ? "<component>"
													 : condition.component };
	return condition.required ? "Has " + label : "Doesn't Have " + label;
}

[[nodiscard]] std::string ComponentQuerySummary(const ComponentEntityQuery& query) {
	if (query.groups.empty()) {
		return "No component query";
	}

	std::string output;

	for (std::size_t group_index{ 0 }; group_index < query.groups.size(); ++group_index) {
		if (group_index > 0) {
			output += " OR ";
		}

		const auto& group{ query.groups[group_index] };

		if (query.groups.size() > 1) {
			output += "(";
		}

		if (group.conditions.empty()) {
			output += "<empty>";
		} else {
			for (std::size_t condition_index{ 0 }; condition_index < group.conditions.size();
				 ++condition_index) {
				if (condition_index > 0) {
					output += " AND ";
				}

				output += ComponentConditionSummary(group.conditions[condition_index]);
			}
		}

		if (query.groups.size() > 1) {
			output += ")";
		}
	}

	return output;
}

[[nodiscard]] bool HasValidComponentCondition(const ComponentEntityQuery& query) {
	for (const auto& group : query.groups) {
		for (const auto& condition : group.conditions) {
			if (!condition.component.empty() &&
				ComponentRegistry::Find(std::string_view{ condition.component })) {
				return true;
			}
		}
	}

	return false;
}

[[nodiscard]] std::string FilterSummary(
	Scene* scene, [[maybe_unused]] Entity owner, const EntityFilter& target
) {
	switch (target.type) {
		case EntityFilterType::None:   return "No Entities";
		case EntityFilterType::Any:	   return "All Entities";

		case EntityFilterType::Entity: {
			if (!scene) {
				return target.entity.uuid.has_value()
						 ? (target.entity.tag.empty() ? "Selected Entity" : target.entity.tag)
						 : "Entity: None";
			}

			Entity entity{ ResolveEntity(*scene, target.entity) };

			return entity ? EntityDisplayName(entity) : "Entity: None";
		}

		case EntityFilterType::Components: {
			if (!HasValidComponentCondition(target.components)) {
				return "Components: None";
			}

			std::string summary{ ComponentQuerySummary(target.components) };

			if (summary.size() <= 52) {
				return summary;
			}

			std::size_t condition_count{ 0 };

			for (const auto& group : target.components.groups) {
				condition_count += group.conditions.size();
			}

			return std::to_string(condition_count) +
				   (condition_count == 1 ? " component condition" : " component conditions");
		}

		case EntityFilterType::Tags:
			if (target.tags.tags.empty()) {
				return "Tags: None";
			}

			if (target.tags.tags.size() == 1) {
				return "Tags: " + target.tags.tags.front();
			}

			return std::to_string(target.tags.tags.size()) + " tags";

		case EntityFilterType::Query: {
			const auto* query{ EntityQueryRegistry::Find(target.query.key) };

			return query ? query->label : "Query: None";
		}
	}

	return "Target";
}

void DrawMatchPreview(const std::vector<Entity>& matches) {
	std::string label{ std::to_string(matches.size()) +
					   (matches.size() == 1 ? " match" : " matches") };
	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth };

	if (matches.empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	bool open{ ImGui::TreeNodeEx("##Matches", flags, "%s", label.c_str()) };

	if (matches.empty() || !open) {
		return;
	}

	for (Entity entity : matches) {
		std::string row{ EntityDisplayName(entity) + " [" + UUIDDisplayName(entity) + "]" };
		ImGui::BulletText("%s", row.c_str());
	}

	ImGui::TreePop();
}

bool DrawConditionType(ComponentQueryCondition& condition) {
	const char* label{ condition.required ? "Has" : "Doesn't Have" };

	if (!ImGui::Button(
			label, ImVec2{ ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() }
		)) {
		return false;
	}

	condition.required = !condition.required;

	return true;
}

bool DrawComponentQueryEditor(
	Scene* scene, ComponentEntityQuery& query, EntityFilterEditorState& state
) {
	bool changed{ false };
	std::optional<std::size_t> group_to_remove;

	for (std::size_t group_index{ 0 }; group_index < query.groups.size(); ++group_index) {
		auto& group{ query.groups[group_index] };

		if (group_index > 0) {
			ImGui::TextDisabled("OR");
		}

		ImGui::PushID(static_cast<int>(group_index));

		std::optional<std::size_t> condition_to_remove;
		ImGui::PushStyleVar(
			ImGuiStyleVar_CellPadding, ImVec2{ ImGui::GetStyle().ItemInnerSpacing.x * 0.5f, 0.0f }
		);

		if (ImGui::BeginTable("##ComponentQueryGroup", 4, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Join", ImGuiTableColumnFlags_WidthFixed, 34.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 118.0f);
			ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight()
			);

			for (std::size_t condition_index{ 0 }; condition_index < group.conditions.size();
				 ++condition_index) {
				auto& condition{ group.conditions[condition_index] };
				ImGui::PushID(static_cast<int>(condition_index));
				ImGui::TableNextRow(0, ImGui::GetFrameHeight() + kComponentRowExtraHeight);
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();

				if (condition_index > 0) {
					ImGui::TextDisabled("AND");
				}

				ImGui::TableSetColumnIndex(1);
				changed |= DrawConditionType(condition);
				ImGui::TableSetColumnIndex(2);
				changed |= DrawComponentPicker("##ComponentPicker", condition.component, state);
				ImGui::TableSetColumnIndex(3);

				if (ImGui::Button(
						"X", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }
					)) {
					if (group.conditions.size() == 1) {
						group_to_remove = group_index;
					} else {
						condition_to_remove = condition_index;
					}
				}

				ImGui::PopID();

				if (group_to_remove.has_value() || condition_to_remove.has_value()) {
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
				group.conditions.begin() + static_cast<std::ptrdiff_t>(condition_to_remove.value())
			);
			changed = true;
		}

		float row_height{ ImGui::GetFrameHeight() };

		if (ImGui::Button("+ AND", ImVec2{ 0.0f, row_height })) {
			group.conditions.push_back(ComponentQueryCondition{});
			changed = true;
		}

		if (query.groups.size() > 1) {
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);

			if (ImGui::Button("Remove Group", ImVec2{ 0.0f, row_height })) {
				group_to_remove = group_index;
			}
		}

		ImGui::PopID();

		if (group_to_remove.has_value()) {
			break;
		}
	}

	if (group_to_remove.has_value()) {
		query.groups.erase(
			query.groups.begin() + static_cast<std::ptrdiff_t>(group_to_remove.value())
		);
		changed = true;
	}

	if (ImGui::Button("+ OR Group", ImVec2{ 0.0f, ImGui::GetFrameHeight() })) {
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

[[nodiscard]] std::set<std::string> GetSceneTags(Scene* scene) {
	std::set<std::string> tags;

	if (!scene) {
		return tags;
	}

	for (const auto& name : scene->GetTags()) {
		tags.insert(name);
	}

	for (Entity entity : scene->Entities()) {
		auto* membership{ entity.TryGet<Tags>() };

		if (!membership) {
			continue;
		}

		for (const auto& tag : membership->values) {
			if (!tag.empty()) {
				tags.insert(tag);
			}
		}
	}

	return tags;
}

bool DrawTagPicker(Scene* scene, TagEntityQuery& query, EntityFilterEditorState& state) {
	std::string preview{ "Select Tags" };

	if (!query.tags.empty()) {
		preview = "Tags: ";

		for (std::size_t i{ 0 }; i < query.tags.size(); ++i) {
			if (i > 0) {
				preview += ", ";
			}

			preview += query.tags[i];
		}
	}

	bool changed{ false };
	ImGui::BeginGroup();
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool open{ ImGui::BeginCombo("##TagPicker", preview.c_str()) };

	if (open) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##TagSearch", "Search tags...", &state.tag_filter);
		auto tags{ GetSceneTags(scene) };
		bool any_visible{ false };

		for (const auto& tag : tags) {
			if (!ContainsCaseInsensitive(tag, state.tag_filter)) {
				continue;
			}

			any_visible = true;
			bool selected{ std::ranges::contains(query.tags, tag) };

			if (ImGui::Selectable(tag.c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
				if (selected) {
					std::erase(query.tags, tag);
				} else {
					query.tags.push_back(tag);
				}

				changed = true;
			}
		}

		std::string custom_tag{ TrimWhitespace(state.tag_filter) };
		bool can_add_custom_tag{ !custom_tag.empty() &&
								 !std::ranges::contains(query.tags, custom_tag) &&
								 !tags.contains(custom_tag) };
		if (can_add_custom_tag) {
			if (any_visible) {
				ImGui::Separator();
			}

			std::string add_label{ "+ Add \"" + custom_tag + "\"" };

			if (ImGui::Selectable(add_label.c_str())) {
				query.tags.push_back(custom_tag);
				if (scene) {
					scene->AddTag(custom_tag);
				}
				state.tag_filter.clear();
				changed = true;
			}
		} else if (!any_visible) {
			ImGui::TextDisabled(
				scene ? "No matching tags. Create one using Add Tag." : "Type a tag name to add it."
			);
		}

		ImGui::EndCombo();
	}

	ImGui::EndGroup();

	if (ImGui::IsItemHovered()) {
		if (query.tags.empty()) {
			ImGui::SetTooltip("No tags selected. Choose one or more tags to match.");
		} else {
			ImGui::SetTooltip(
				"%s\nMatches entities that belong to any selected tag.", preview.c_str()
			);
		}
	}

	return changed;
}

bool DrawTagQueryEditor(Scene* scene, TagEntityQuery& query, EntityFilterEditorState& state) {
	bool changed{ DrawTagPicker(scene, query, state) };

	if (scene) {
		DrawMatchPreview(ResolveTagQuery(*scene, query));
	}

	return changed;
}

bool DrawRegisteredQueryPicker(
	RegisteredEntityQueryReference& query, EntityFilterEditorState& state
) {
	const auto* selected{ EntityQueryRegistry::Find(query.key) };
	std::string preview{ selected ? selected->label : "Select Query" };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (!ImGui::BeginCombo("##QueryPicker", preview.c_str())) {
		return false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##QuerySearch", "Search queries...", &state.query_filter);
	auto queries{ EntityQueryRegistry::Queries() };
	std::ranges::sort(queries, [](const auto& lhs, const auto& rhs) {
		if (lhs.group != rhs.group) {
			return lhs.group < rhs.group;
		}

		return lhs.label < rhs.label;
	});
	std::vector<std::string> visible_groups;
	bool has_ungrouped{ false };
	auto visible = [&](const RegisteredEntityQuery& registration) {
		return ContainsCaseInsensitive(registration.label, state.query_filter) ||
			   ContainsCaseInsensitive(registration.group, state.query_filter) ||
			   ContainsCaseInsensitive(registration.description, state.query_filter);
	};

	for (const auto& registration : queries) {
		if (!visible(registration)) {
			continue;
		}

		if (registration.group.empty()) {
			has_ungrouped = true;
			continue;
		}

		if (!std::ranges::contains(visible_groups, registration.group)) {
			visible_groups.push_back(registration.group);
		}
	}

	bool any_visible{ has_ungrouped || !visible_groups.empty() };

	for (const auto& registration : queries) {
		if (!registration.group.empty() || !visible(registration)) {
			continue;
		}

		if (ImGui::Selectable(registration.label.c_str(), query.key == registration.key)) {
			query.key = registration.key;
			changed	  = true;
		}

		if (ImGui::IsItemHovered() && !registration.description.empty()) {
			ImGui::SetTooltip("%s", registration.description.c_str());
		}
	}

	for (const auto& group : visible_groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}

		for (const auto& registration : queries) {
			if (registration.group != group || !visible(registration)) {
				continue;
			}

			if (ImGui::MenuItem(
					registration.label.c_str(), nullptr, query.key == registration.key
				)) {
				query.key = registration.key;
				changed	  = true;
			}

			if (ImGui::IsItemHovered() && !registration.description.empty()) {
				ImGui::SetTooltip("%s", registration.description.c_str());
			}
		}

		ImGui::EndMenu();
	}

	if (!any_visible) {
		ImGui::TextDisabled("No matching queries.");
	}

	ImGui::EndCombo();

	return changed;
}

bool DrawRegisteredQueryEditor(
	Scene* scene, Entity owner, RegisteredEntityQueryReference& query,
	EntityFilterEditorState& state
) {
	bool changed{ DrawRegisteredQueryPicker(query, state) };
	const auto* selected{ EntityQueryRegistry::Find(query.key) };

	if (selected && !selected->description.empty()) {
		ImGui::TextDisabled("%s", selected->description.c_str());
	}

	if (scene) {
		DrawMatchPreview(ResolveRegisteredQuery(*scene, owner, query));
	}

	return changed;
}

bool DrawEntityFilterEditor(
	Scene* scene, Entity owner, EntityFilter& target, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options
) {
	bool changed{ false };

	auto mode_visible = [&](EntityFilterType type) {
		if (options.single_entity_only) {
			return type == EntityFilterType::Entity;
		}
		switch (type) {
			case EntityFilterType::None:	   return options.show_none;
			case EntityFilterType::Any:		   return options.show_any;
			case EntityFilterType::Entity:	   return options.show_entity;
			case EntityFilterType::Components: return options.show_components;
			case EntityFilterType::Tags:	   return options.show_tags;
			case EntityFilterType::Query:	   return options.show_queries;
		}

		return false;
	};

	if (!mode_visible(target.type)) {
		if (options.single_entity_only || options.show_entity) {
			target.type = EntityFilterType::Entity;
		} else if (options.show_any) {
			target.type = EntityFilterType::Any;
		} else if (options.show_components) {
			target.type = EntityFilterType::Components;
		} else if (options.show_tags) {
			target.type = EntityFilterType::Tags;
		} else if (options.show_queries) {
			target.type = EntityFilterType::Query;
		} else if (options.show_none) {
			target.type = EntityFilterType::None;
		}
	}

	std::size_t mode_count{ options.single_entity_only
								? 1
								: static_cast<std::size_t>(options.show_none) +
									  static_cast<std::size_t>(options.show_any) +
									  static_cast<std::size_t>(options.show_entity) +
									  static_cast<std::size_t>(options.show_components) +
									  static_cast<std::size_t>(options.show_tags) +
									  static_cast<std::size_t>(options.show_queries) };
	ImGuiStyle& style{ ImGui::GetStyle() };
	float spacing{ style.ItemSpacing.x };
	float mode_height{ ImGui::GetFrameHeight() };

	if (mode_count > 1 && !options.single_entity_only) {
		float vertical_spacing{ style.ItemInnerSpacing.y };

		if (style.WindowPadding.y > vertical_spacing) {
			ImGui::SetCursorPosY(
				ImGui::GetCursorPosY() - (style.WindowPadding.y - vertical_spacing)
			);
		}

		ImGui::PushStyleVar(
			ImGuiStyleVar_ItemSpacing, ImVec2{ style.ItemSpacing.x, vertical_spacing }
		);
		float available{ ImGui::GetContentRegionAvail().x };
		float gaps{ spacing * static_cast<float>(mode_count - 1) };
		float mode_width{ std::max(64.0f, (available - gaps) / static_cast<float>(mode_count)) };
		bool first{ true };
		auto draw_mode = [&](const char* label, EntityFilterType type, bool visible) {
			if (!visible) {
				return;
			}

			if (!first) {
				ImGui::SameLine(0.0f, spacing);
			}

			first = false;
			bool disabled{ (type == EntityFilterType::Entity) && !scene };
			ImGui::BeginDisabled(disabled);

			if (ImGui::Selectable(
					label, target.type == type, ImGuiSelectableFlags_DontClosePopups,
					ImVec2{ mode_width, mode_height }
				)) {
				target.type = type;

				if (type == EntityFilterType::Components && target.components.groups.empty()) {
					target.components.groups.push_back(
						ComponentQueryGroup{ .conditions{ ComponentQueryCondition{} } }
					);
				}

				changed = true;
			}

			ImGui::EndDisabled();
		};
		ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2{ 0.5f, 0.5f });
		draw_mode("None", EntityFilterType::None, options.show_none);
		draw_mode("All", EntityFilterType::Any, options.show_any);
		draw_mode("Entity", EntityFilterType::Entity, options.show_entity);
		draw_mode("Components", EntityFilterType::Components, options.show_components);
		draw_mode("Tags", EntityFilterType::Tags, options.show_tags);
		draw_mode("Queries", EntityFilterType::Query, options.show_queries);
		ImGui::PopStyleVar();
		ImGui::Separator();
		ImGui::PopStyleVar();
	} else if (!options.single_entity_only) {
		ImGui::Separator();
	}

	switch (target.type) {
		case EntityFilterType::None: ImGui::TextDisabled("No entities match this filter."); break;
		case EntityFilterType::Any:	 ImGui::TextDisabled("All entities match this filter."); break;
		case EntityFilterType::Entity:
			if (scene) {
				changed |=
					DrawMiniHierarchy(*scene, owner, std::addressof(target.entity), state, options);
			} else {
				ImGui::TextDisabled("Exact entity selection requires a scene instance.");
			}

			break;
		case EntityFilterType::Components:
			changed |= DrawComponentQueryEditor(scene, target.components, state);
			break;
		case EntityFilterType::Tags:
			changed |= DrawTagQueryEditor(scene, target.tags, state);
			break;
		case EntityFilterType::Query:
			changed |= DrawRegisteredQueryEditor(scene, owner, target.query, state);
			break;
	}

	return changed;
}

void DrawFilterButtonTooltip(
	Scene* scene, const EntityFilter& target, const char* prefix = nullptr
) {
	if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		return;
	}

	auto set_tooltip = [prefix](const std::string& detail) {
		if (prefix && *prefix) {
			ImGui::SetTooltip("%s\n%s", prefix, detail.c_str());
		} else {
			ImGui::SetTooltip("%s", detail.c_str());
		}
	};

	switch (target.type) {
		case EntityFilterType::Entity:
			if (target.entity.uuid.has_value()) {
				if (scene) {
					Entity entity{ ResolveEntity(*scene, target.entity) };
					if (entity) {
						set_tooltip(
							EntityDisplayName(entity) + "\nuuid: " + UUIDDisplayName(entity)
						);
						return;
					}
				} else {
					json uuid = target.entity.uuid.value();
					std::string uuid_text{ uuid.is_string() ? uuid.get<std::string>()
															: uuid.dump() };
					set_tooltip(
						(target.entity.tag.empty() ? std::string{} : target.entity.tag + "\n") +
						"uuid: " + uuid_text
					);
					return;
				}
			}
			break;

		case EntityFilterType::Components:
			if (HasValidComponentCondition(target.components)) {
				set_tooltip(ComponentQuerySummary(target.components));
				return;
			}
			break;

		case EntityFilterType::Tags:
			if (!target.tags.tags.empty()) {
				std::string groups{ "Tags: " };
				for (std::size_t i{ 0 }; i < target.tags.tags.size(); ++i) {
					if (i > 0) {
						groups += ", ";
					}
					groups += target.tags.tags[i];
				}
				set_tooltip(groups);
				return;
			}
			break;

		default: break;
	}

	set_tooltip(FilterSummary(scene, {}, target));
}

void ApplyPopupConstraints(const EntityFilterEditorOptions& options) {
	float popup_width{ GetPopupWidth(options) };
	float popup_max_height{ FLT_MAX };

	if (auto* viewport{ ImGui::GetWindowViewport() }) {
		popup_width = std::min(
			popup_width, std::max(1.0f, viewport->WorkSize.x - kTargetPopupScreenMargin * 2.0f)
		);
		popup_max_height = std::max(1.0f, viewport->WorkSize.y - kTargetPopupScreenMargin * 2.0f);
	}

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ popup_width, 0.0f }, ImVec2{ popup_width, popup_max_height }
	);
}

void ClampCurrentPopupToViewport() {
	auto* viewport{ ImGui::GetWindowViewport() };

	if (!viewport) {
		return;
	}

	ImVec2 popup_position{ ImGui::GetWindowPos() };
	ImVec2 popup_size{ ImGui::GetWindowSize() };
	float min_x{ viewport->WorkPos.x + kTargetPopupScreenMargin };
	float min_y{ viewport->WorkPos.y + kTargetPopupScreenMargin };
	float max_x{ viewport->WorkPos.x + viewport->WorkSize.x - popup_size.x -
				 kTargetPopupScreenMargin };
	float max_y{ viewport->WorkPos.y + viewport->WorkSize.y - popup_size.y -
				 kTargetPopupScreenMargin };
	max_x = std::max(max_x, min_x);
	max_y = std::max(max_y, min_y);
	ImVec2 clamped_position{ std::clamp(popup_position.x, min_x, max_x),
							 std::clamp(popup_position.y, min_y, max_y) };
	if (clamped_position.x != popup_position.x || clamped_position.y != popup_position.y) {
		ImGui::SetWindowPos(clamped_position, ImGuiCond_Always);
	}
}

bool BeginEntityFilterPopup(const EntityFilterEditorOptions& options) {
	ApplyPopupConstraints(options);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

	bool popup_open{ true };
	bool open{ ImGui::BeginPopupModal(
		"Entity Filter###EntityFilterPopup", &popup_open,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse
	) };
	ImGui::PopStyleColor();

	return open;
}

bool DrawEntityFilterButtonImpl(
	Scene* scene, Entity owner, EntityFilter& target, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options
) {
	ImGui::PushID(std::addressof(target));

	std::string summary{ FilterSummary(scene, owner, target) };
	bool changed{ false };

	float button_width{ options.button_width > 0.0f ? options.button_width
													: ImGui::GetContentRegionAvail().x };
	if (ImGui::Button(summary.c_str(), ImVec2{ button_width, 0.0f })) {
		ImGui::OpenPopup("Entity Filter###EntityFilterPopup");
	}

	if (options.append_filter_tooltip) {
		DrawFilterButtonTooltip(scene, target, options.button_tooltip);
	} else if (options.button_tooltip) {
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", options.button_tooltip);
		}
	} else {
		DrawFilterButtonTooltip(scene, target);
	}

	if (BeginEntityFilterPopup(options)) {
		changed |= DrawEntityFilterEditor(scene, owner, target, state, options);
		ClampCurrentPopupToViewport();
		ImGui::EndPopup();
	}

	ImGui::PopID();

	return changed;
}

bool DrawEntityFilterButtonImpl(
	Scene* scene, Entity owner, std::optional<EntityFilter>& target, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options
) {
	ImGui::PushID(std::addressof(target));

	bool owner_target{ !target || (target->type == EntityFilterType::Entity &&
								   IsOwnerReference(owner, target->entity)) };
	std::string summary{ owner_target ? "Owner" : FilterSummary(scene, owner, *target) };
	bool changed{ false };

	float button_width{ options.button_width > 0.0f ? options.button_width
													: ImGui::GetContentRegionAvail().x };
	if (ImGui::Button(summary.c_str(), ImVec2{ button_width, 0.0f })) {
		ImGui::OpenPopup("Entity Filter###EntityFilterPopup");
	}

	if (options.append_filter_tooltip) {
		if (owner_target) {
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				std::string details{ owner ? EntityDisplayName(owner) + "\nuuid: " +
												 UUIDDisplayName(owner) + "\n[sequence owner]"
										   : "No owner entity" };
				if (options.button_tooltip && *options.button_tooltip) {
					ImGui::SetTooltip("%s\n%s", options.button_tooltip, details.c_str());
				} else {
					ImGui::SetTooltip("%s", details.c_str());
				}
			}
		} else {
			DrawFilterButtonTooltip(scene, *target, options.button_tooltip);
		}
	} else if (options.button_tooltip) {
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", options.button_tooltip);
		}
	} else if (owner_target) {
		if (ImGui::IsItemHovered()) {
			if (owner) {
				ImGui::SetTooltip(
					"%s\nuuid: %s\n[sequence owner]", EntityDisplayName(owner).c_str(),
					UUIDDisplayName(owner).c_str()
				);
			} else {
				ImGui::SetTooltip("No owner entity");
			}
		}
	} else {
		DrawFilterButtonTooltip(scene, *target);
	}

	if (BeginEntityFilterPopup(options)) {
		EntityFilter edited{ target.value_or(
			EntityFilter{
				.type = EntityFilterType::Entity,
			}
		) };

		if (!target && owner) {
			edited.type = EntityFilterType::Entity;
			SetEntityRef(edited.entity, owner);
		}

		bool local_changed{ DrawEntityFilterEditor(scene, owner, edited, state, options) };

		if (local_changed) {
			if (edited.type == EntityFilterType::Entity && IsOwnerReference(owner, edited.entity)) {
				target.reset();
			} else {
				target = std::move(edited);
			}

			changed = true;
		}

		ClampCurrentPopupToViewport();
		ImGui::EndPopup();
	}

	ImGui::PopID();

	return changed;
}

} // namespace

bool DrawEntityFilterButton(
	Scene* scene, Entity owner, EntityFilter& filter, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options
) {
	return DrawEntityFilterButtonImpl(scene, owner, filter, state, options);
}

bool DrawEntityFilterButton(
	Scene* scene, Entity owner, std::optional<EntityFilter>& filter, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options
) {
	return DrawEntityFilterButtonImpl(scene, owner, filter, state, options);
}

} // namespace ptgn::editor::inspector
