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
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "panels/entity_hierarchy.h"
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

using EntityReference				 = ::ptgn::EntityReference;
using SceneLayerReference			 = ::ptgn::SceneLayerReference;
using ComponentQueryCondition		 = ::ptgn::ComponentQueryCondition;
using ComponentQueryGroup			 = ::ptgn::ComponentQueryGroup;
using ComponentEntityQuery			 = ::ptgn::ComponentEntityQuery;
using GroupEntityQuery				 = ::ptgn::GroupEntityQuery;
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

[[nodiscard]] char LowerAscii(char c) {
	if (c >= 'A' && c <= 'Z') {
		return static_cast<char>(c - 'A' + 'a');
	}

	return c;
}

[[nodiscard]] std::string TrimWhitespace(std::string value) {
	auto first{ std::ranges::find_if(value, [](char c) {
		return !std::isspace(static_cast<unsigned char>(c));
	}) };

	if (first == value.end()) {
		return {};
	}

	auto last{ std::ranges::find_if(value | std::views::reverse, [](char c) {
				   return !std::isspace(static_cast<unsigned char>(c));
			   }).base() };
	return std::string{ first, last };
}

[[nodiscard]] bool ContainsCaseInsensitive(std::string_view text, std::string_view filter) {
	if (filter.empty()) {
		return true;
	}

	if (filter.size() > text.size()) {
		return false;
	}

	for (std::size_t i{ 0 }; i + filter.size() <= text.size(); ++i) {
		bool matches{ true };

		for (std::size_t j{ 0 }; j < filter.size(); ++j) {
			if (LowerAscii(text[i + j]) != LowerAscii(filter[j])) {
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

[[nodiscard]] bool EntityOrDescendantMatchesFilter(
	Entity entity, std::string_view filter_text, std::size_t depth = 0
) {
	if (!entity || depth >= kMaxHierarchyDepth) {
		return false;
	}

	if (hierarchy::MatchesFilter(entity, filter_text)) {
		return true;
	}

	if (!HasChildren(entity)) {
		return false;
	}

	for (Entity child : GetChildren(entity)) {
		if (EntityOrDescendantMatchesFilter(child, filter_text, depth + 1)) {
			return true;
		}
	}

	return false;
}

[[nodiscard]] bool IsHierarchyEntityExcluded(
	Entity entity, Entity excluded_entity, std::span<const Entity> excluded_entities
) {
	return entity == excluded_entity || std::ranges::contains(excluded_entities, entity);
}

void SetLayerRef(SceneLayerReference& reference, const SceneLayer& layer) {
	reference.id   = layer.id;
	reference.name = layer.name;
}

[[nodiscard]] const SceneLayer* ResolveLayer(Scene& scene, const SceneLayerReference& reference) {
	return reference.id ? scene.GetLayers().Find(reference.id) : nullptr;
}

void EnsureLayerRef(Scene& scene, Entity owner, SceneLayerReference& reference) {
	if (ResolveLayer(scene, reference)) {
		return;
	}

	auto& layers{ scene.GetLayers() };

	if (owner) {
		if (const SceneLayer* owner_layer{ layers.GetLayer(owner) }) {
			SetLayerRef(reference, *owner_layer);
			return;
		}
	}

	if (const SceneLayer* default_layer{ layers.Find(layers.GetDefaultEntityLayer()) }) {
		SetLayerRef(reference, *default_layer);
	}
}

[[nodiscard]] bool MatchesLayerFilter(const SceneLayer& layer, std::string_view filter_text) {
	if (filter_text.empty()) {
		return true;
	}

	std::string name{ layer.name };
	std::ranges::transform(name, name.begin(), LowerAscii);

	std::string filter{ filter_text };
	bool has_name_include{ false };
	bool matched_name_include{ false };
	bool has_kind_include{ false };
	bool matched_kind_include{ false };
	std::size_t start{ 0 };

	while (start <= filter.size()) {
		std::size_t comma{ filter.find(',', start) };
		std::string token{ comma == std::string::npos ? filter.substr(start)
													  : filter.substr(start, comma - start) };
		token = TrimWhitespace(std::move(token));
		std::ranges::transform(token, token.begin(), LowerAscii);

		if (!token.empty()) {
			bool exclude{ token.front() == '-' };
			std::string criterion{ TrimWhitespace(exclude ? token.substr(1) : token) };

			if (!criterion.empty()) {
				bool is_kind_filter{ criterion.front() == '*' };
				std::string kind{ is_kind_filter ? TrimWhitespace(criterion.substr(1))
												 : std::string{} };

				is_kind_filter &= kind == "entity" || kind == "tile";

				if (is_kind_filter) {
					bool kind_matches{ (kind == "entity" && layer.kind == SceneLayerKind::Entity) ||
									   (kind == "tile" && layer.kind == SceneLayerKind::Tile) };

					if (exclude && kind_matches) {
						return false;
					}

					if (!exclude) {
						has_kind_include	  = true;
						matched_kind_include |= kind_matches;
					}
				} else {
					bool contains{ name.find(criterion) != std::string::npos };

					if (exclude && contains) {
						return false;
					}

					if (!exclude) {
						has_name_include	  = true;
						matched_name_include |= contains;
					}
				}
			}
		}

		if (comma == std::string::npos) {
			break;
		}

		start = comma + 1;
	}

	return (!has_name_include || matched_name_include) &&
		   (!has_kind_include || matched_kind_include);
}

void DrawLayerFilterTooltip() {
	if (!ImGui::IsItemHovered()) {
		return;
	}

	ImGui::BeginTooltip();
	ImGui::TextUnformatted("Layer filter syntax:");
	ImGui::Separator();
	ImGui::TextUnformatted("background");
	ImGui::SameLine();
	ImGui::TextDisabled("Name contains \"background\"");
	ImGui::TextUnformatted("-debug");
	ImGui::SameLine();
	ImGui::TextDisabled("Name does not contain \"debug\"");
	ImGui::TextUnformatted("*entity");
	ImGui::SameLine();
	ImGui::TextDisabled("Entity layers only");
	ImGui::TextUnformatted("*tile");
	ImGui::SameLine();
	ImGui::TextDisabled("Tile layers only");
	ImGui::Spacing();
	ImGui::TextDisabled("Separate filters with commas.");
	ImGui::TextDisabled("Positive names use OR; layer type and name filters must both match.");
	ImGui::EndTooltip();
}

void DrawEntityHierarchyNode(
	Entity entity, const EntityReference* current, std::string_view filter, Entity* picked_entity,
	Entity excluded_entity = {}, std::span<const Entity> excluded_entities = {},
	std::size_t depth = 0
) {
	if (!entity || depth >= kMaxHierarchyDepth) {
		return;
	}

	if (IsHierarchyEntityExcluded(entity, excluded_entity, excluded_entities)) {
		if (HasChildren(entity)) {
			auto children{ GetChildren(entity) };
			SortByLocalDepth(children);

			for (Entity child : children) {
				DrawEntityHierarchyNode(
					child, current, filter, picked_entity, excluded_entity, excluded_entities, depth
				);
			}
		}

		return;
	}

	if (!EntityOrDescendantMatchesFilter(entity, filter, depth)) {
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
			if (IsHierarchyEntityExcluded(child, excluded_entity, excluded_entities)) {
				self(self, child);
				continue;
			}

			if (EntityOrDescendantMatchesFilter(child, filter, depth + 1)) {
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
				child, current, filter, picked_entity, excluded_entity, excluded_entities, depth + 1
			);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

bool DrawMiniHierarchy(
	Scene& scene, Entity owner, EntityReference* entity_reference,
	SceneLayerReference* layer_reference, EntityFilterEditorState& state,
	bool allow_select_owner = true, Entity excluded_entity = {},
	std::span<const Entity> excluded_entities = {}
) {
	bool changed{ false };
	auto& layers{ scene.GetLayers() };

	if (layer_reference) {
		EnsureLayerRef(scene, owner, *layer_reference);

		const SceneLayer* selected{ ResolveLayer(scene, *layer_reference) };

		std::string selected_label{ "Selected Layer: " + selected->name };
		ImGui::TextUnformatted(selected_label.c_str());

		if (allow_select_owner && owner) {
			if (const SceneLayer* owner_layer{ layers.GetLayer(owner) };
				owner_layer && ImGui::Button("Select Owner Layer")) {
				SetLayerRef(*layer_reference, *owner_layer);
				changed = true;
			}
		}

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint(
			"##LayerFilter", "Filter: background, -debug, *entity, *tile", &state.layer_filter
		);
		DrawLayerFilterTooltip();

		ImGui::BeginChild(
			"##LayerList", ImVec2{ 0.0f, kEntityHierarchyHeight }, ImGuiChildFlags_Borders
		);

		bool any_visible{ false };

		for (const SceneLayer& layer : layers.GetLayers()) {
			if (!MatchesLayerFilter(layer, state.layer_filter)) {
				continue;
			}

			any_visible = true;
			ImGui::PushID(static_cast<int>(layer.id.value));
			hierarchy::DrawLayerControls(
				layer, hierarchy::LayerControlsOptions{
						   .show_visibility		   = false,
						   .visibility_interactive = false,
					   }
			);

			bool selected_layer{ layer_reference->id == layer.id };

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x * 0.5f);

			if (ImGui::Selectable(layer.name.c_str(), selected_layer)) {
				SetLayerRef(*layer_reference, layer);
				changed = true;
			}

			ImGui::PopID();
		}

		if (!any_visible) {
			ImGui::TextDisabled("No matching layers.");
		}

		ImGui::EndChild();

		return changed;
	}

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

	for (const SceneLayer& layer : layers.GetLayers()) {
		ImGui::PushID(static_cast<int>(layer.id.value));

		auto row{ hierarchy::DrawLayerRow(
            layer,
            hierarchy::LayerRowOptions{
                .controls = {
                    .show_visibility = false,
                    .visibility_interactive = false,
                },
            }
        ) };

		if (row.open) {
			auto roots{ layers.GetRootEntities(scene, layer.id) };
			std::erase_if(roots, [&](Entity entity) {
				return !entity || !scene.Entities().Contains(entity);
			});
			SortByLocalDepth(roots);

			for (Entity root : roots) {
				DrawEntityHierarchyNode(
					root, entity_reference, state.hierarchy_filter, std::addressof(picked_entity),
					excluded_entity, excluded_entities
				);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
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
	float width{ 0.0f };

	if (options.show_entity || options.show_layer) {
		width = std::max(width, kEntityModeMinWidth);
	}

	if (options.show_components) {
		width = std::max(width, kComponentsModeMinWidth);
	}

	if (options.show_groups) {
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

[[nodiscard]] std::vector<Entity> ResolveGroupQuery(Scene& scene, const GroupEntityQuery& query) {
	std::vector<Entity> matches;

	for (Entity entity : scene.Entities()) {
		auto* membership{ entity.TryGet<Group>() };

		if (!membership) {
			continue;
		}

		bool matches_group{ std::ranges::any_of(
			query.groups, [&membership](const std::string& group) {
				return !group.empty() && std::ranges::contains(membership->groups, group);
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

		case EntityFilterType::Layer: {
			if (!scene) {
				return target.layer.id ? "Layer: " + target.layer.name : "Layer: None";
			}

			const SceneLayer* layer{ ResolveLayer(*scene, target.layer) };

			return layer ? "Layer: " + layer->name : "Layer: None";
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

		case EntityFilterType::Group:
			if (target.group.groups.empty()) {
				return "Group: None";
			}

			if (target.group.groups.size() == 1) {
				return "Group: " + target.group.groups.front();
			}

			return std::to_string(target.group.groups.size()) + " groups";

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

[[nodiscard]] std::set<std::string> GetSceneGroups(Scene* scene) {
	std::set<std::string> groups;

	if (!scene) {
		return groups;
	}

	for (Entity entity : scene->Entities()) {
		auto* membership{ entity.TryGet<Group>() };

		if (!membership) {
			continue;
		}

		for (const auto& group : membership->groups) {
			if (!group.empty()) {
				groups.insert(group);
			}
		}
	}

	return groups;
}

bool DrawGroupPicker(Scene* scene, GroupEntityQuery& query, EntityFilterEditorState& state) {
	std::string preview{ "Select Groups" };

	if (!query.groups.empty()) {
		preview = "Groups: ";

		for (std::size_t i{ 0 }; i < query.groups.size(); ++i) {
			if (i > 0) {
				preview += ", ";
			}

			preview += query.groups[i];
		}
	}

	bool changed{ false };
	ImGui::BeginGroup();
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool open{ ImGui::BeginCombo("##GroupPicker", preview.c_str()) };

	if (open) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##GroupSearch", "Search groups...", &state.group_filter);
		auto groups{ GetSceneGroups(scene) };
		bool any_visible{ false };

		for (const auto& group : groups) {
			if (!ContainsCaseInsensitive(group, state.group_filter)) {
				continue;
			}

			any_visible = true;
			bool selected{ std::ranges::contains(query.groups, group) };

			if (ImGui::Selectable(group.c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
				if (selected) {
					std::erase(query.groups, group);
				} else {
					query.groups.push_back(group);
				}

				changed = true;
			}
		}

		std::string custom_group{ TrimWhitespace(state.group_filter) };
		bool can_add_custom_group{ !custom_group.empty() &&
								   !std::ranges::contains(query.groups, custom_group) &&
								   !groups.contains(custom_group) };
		if (can_add_custom_group) {
			if (any_visible) {
				ImGui::Separator();
			}

			std::string add_label{ "+ Add \"" + custom_group + "\"" };

			if (ImGui::Selectable(add_label.c_str())) {
				query.groups.push_back(custom_group);
				state.group_filter.clear();
				changed = true;
			}
		} else if (!any_visible) {
			ImGui::TextDisabled(
				scene ? "No matching groups. Type a new group name to add it."
					  : "Type a group name to add it."
			);
		}

		ImGui::EndCombo();
	}

	ImGui::EndGroup();

	if (ImGui::IsItemHovered()) {
		if (query.groups.empty()) {
			ImGui::SetTooltip(
				"No groups selected. Matches entities that belong to any selected group."
			);
		} else {
			ImGui::SetTooltip(
				"%s\nMatches entities that belong to any selected group.", preview.c_str()
			);
		}
	}

	return changed;
}

bool DrawGroupQueryEditor(Scene* scene, GroupEntityQuery& query, EntityFilterEditorState& state) {
	bool changed{ DrawGroupPicker(scene, query, state) };

	if (scene) {
		DrawMatchPreview(ResolveGroupQuery(*scene, query));
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
		switch (type) {
			case EntityFilterType::None:	   return options.show_none;
			case EntityFilterType::Any:		   return options.show_any;
			case EntityFilterType::Entity:	   return options.show_entity;
			case EntityFilterType::Layer:	   return options.show_layer;
			case EntityFilterType::Components: return options.show_components;
			case EntityFilterType::Group:	   return options.show_groups;
			case EntityFilterType::Query:	   return options.show_queries;
		}

		return false;
	};

	if (!mode_visible(target.type)) {
		if (options.show_entity) {
			target.type = EntityFilterType::Entity;
		} else if (options.show_layer) {
			target.type = EntityFilterType::Layer;

			if (scene) {
				EnsureLayerRef(*scene, owner, target.layer);
			}
		} else if (options.show_any) {
			target.type = EntityFilterType::Any;
		} else if (options.show_components) {
			target.type = EntityFilterType::Components;
		} else if (options.show_groups) {
			target.type = EntityFilterType::Group;
		} else if (options.show_queries) {
			target.type = EntityFilterType::Query;
		} else if (options.show_none) {
			target.type = EntityFilterType::None;
		}
	}

	std::size_t mode_count{ static_cast<std::size_t>(options.show_none) +
							static_cast<std::size_t>(options.show_any) +
							static_cast<std::size_t>(options.show_entity) +
							static_cast<std::size_t>(options.show_layer) +
							static_cast<std::size_t>(options.show_components) +
							static_cast<std::size_t>(options.show_groups) +
							static_cast<std::size_t>(options.show_queries) };
	ImGuiStyle& style{ ImGui::GetStyle() };
	float spacing{ style.ItemSpacing.x };
	float mode_height{ ImGui::GetFrameHeight() };

	if (mode_count > 1) {
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
			bool disabled{ (type == EntityFilterType::Entity || type == EntityFilterType::Layer) &&
						   !scene };
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
		draw_mode("Layer", EntityFilterType::Layer, options.show_layer);
		draw_mode("Components", EntityFilterType::Components, options.show_components);
		draw_mode("Groups", EntityFilterType::Group, options.show_groups);
		draw_mode("Queries", EntityFilterType::Query, options.show_queries);
		ImGui::PopStyleVar();
		ImGui::Separator();
		ImGui::PopStyleVar();
	} else {
		ImGui::Separator();
	}

	switch (target.type) {
		case EntityFilterType::None: ImGui::TextDisabled("No entities match this filter."); break;
		case EntityFilterType::Any:	 ImGui::TextDisabled("All entities match this filter."); break;
		case EntityFilterType::Entity:
			if (scene) {
				changed |= DrawMiniHierarchy(
					*scene, owner, std::addressof(target.entity), nullptr, state,
					options.allow_select_owner, options.exclude_owner ? owner : Entity{},
					options.excluded_entities
				);
			} else {
				ImGui::TextDisabled("Exact entity selection requires a scene instance.");
			}

			break;
		case EntityFilterType::Layer:
			if (scene) {
				changed |= DrawMiniHierarchy(
					*scene, owner, nullptr, std::addressof(target.layer), state,
					options.allow_select_owner
				);
			} else {
				ImGui::TextDisabled("Scene layer selection requires a scene instance.");
			}

			break;
		case EntityFilterType::Components:
			changed |= DrawComponentQueryEditor(scene, target.components, state);
			break;
		case EntityFilterType::Group:
			changed |= DrawGroupQueryEditor(scene, target.group, state);
			break;
		case EntityFilterType::Query:
			changed |= DrawRegisteredQueryEditor(scene, owner, target.query, state);
			break;
	}

	return changed;
}

void DrawFilterButtonTooltip(Scene* scene, const EntityFilter& target) {
	if (!ImGui::IsItemHovered()) {
		return;
	}

	switch (target.type) {
		case EntityFilterType::Entity:
			if (target.entity.uuid.has_value()) {
				if (scene) {
					Entity entity{ ResolveEntity(*scene, target.entity) };

					if (entity) {
						std::string name{ EntityDisplayName(entity) };
						std::string uuid{ UUIDDisplayName(entity) };
						ImGui::SetTooltip("%s\nuuid: %s", name.c_str(), uuid.c_str());
						return;
					}
				} else {
					json uuid = target.entity.uuid.value();
					std::string uuid_text{ uuid.is_string() ? uuid.get<std::string>()
															: uuid.dump() };

					if (!target.entity.tag.empty()) {
						ImGui::SetTooltip(
							"%s\nuuid: %s", target.entity.tag.c_str(), uuid_text.c_str()
						);
					} else {
						ImGui::SetTooltip("uuid: %s", uuid_text.c_str());
					}

					return;
				}
			}
			break;

		case EntityFilterType::Layer:
			if (scene) {
				if (const SceneLayer* layer{ ResolveLayer(*scene, target.layer) }) {
					ImGui::SetTooltip("Layer: %s", layer->name.c_str());
					return;
				}
			} else if (!target.layer.name.empty()) {
				ImGui::SetTooltip("Layer: %s", target.layer.name.c_str());
				return;
			}
			break;

		case EntityFilterType::Components: {
			if (!HasValidComponentCondition(target.components)) {
				break;
			}

			std::string full_summary{ ComponentQuerySummary(target.components) };
			ImGui::SetTooltip("%s", full_summary.c_str());
			return;
		}

		case EntityFilterType::Group:
			if (!target.group.groups.empty()) {
				std::string groups{ "Groups: " };

				for (std::size_t i{ 0 }; i < target.group.groups.size(); ++i) {
					if (i > 0) {
						groups += ", ";
					}

					groups += target.group.groups[i];
				}

				ImGui::SetTooltip("%s", groups.c_str());
				return;
			}
			break;

		default: break;
	}

	std::string summary{ FilterSummary(scene, {}, target) };
	ImGui::SetTooltip("%s", summary.c_str());
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

	if (ImGui::Button(summary.c_str(), ImVec2{ ImGui::GetContentRegionAvail().x, 0.0f })) {
		ImGui::OpenPopup("Entity Filter###EntityFilterPopup");
	}

	DrawFilterButtonTooltip(scene, target);

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

	if (ImGui::Button(summary.c_str(), ImVec2{ ImGui::GetContentRegionAvail().x, 0.0f })) {
		ImGui::OpenPopup("Entity Filter###EntityFilterPopup");
	}

	if (owner_target) {
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("The sequence owner is the target.");
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
