// script_sequence_event_ecs_imgui_demo.cpp
//
// Standalone Dear ImGui mock-up for a unified Events + Scripts + Script Sequences system.
//
// Flat sequence entry types:
//   - Sequence Item
//   - Timed Sequence Item
//   - Wait
//   - Emit Event
//
// Also demonstrates:
//   - Prefab creation and reflected component editing.
//   - Configurable single-prefab Spawn Entity action with random placement.
//   - Reflected multi-component Add/Remove Components actions.
//   - Separate start and stop trigger sections.
//   - Lifecycle callbacks and completion cleanup.
//
// Script sequences live in an entity's Scripts component. They can be local or references
// to shared definitions. Authored fields and private runtime state live in one sequence object.
//
// Expected dependencies: Dear ImGui, GLFW, GLAD/OpenGL, and the standard
// imgui_impl_glfw / imgui_impl_opengl3 backends.

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <imgui.h>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace demo {

using Id = std::uint64_t;

Id NextId() {
	static Id next{ 1 };
	return next++;
}

template <std::size_t N>
struct TextBuffer {
	std::array<char, N> data{};

	TextBuffer() = default;
	explicit TextBuffer(std::string_view text) { Assign(text); }

	void Assign(std::string_view text) {
		const auto count{ std::min(text.size(), N - 1) };
		std::memcpy(data.data(), text.data(), count);
		data[count] = '\0';
		if (count + 1 < N) {
			std::fill(data.begin() + static_cast<std::ptrdiff_t>(count + 1), data.end(), '\0');
		}
	}

	char* Data() { return data.data(); }
	const char* Data() const { return data.data(); }
	constexpr std::size_t Size() const { return N; }
	bool Empty() const { return data[0] == '\0'; }
	std::string_view View() const { return Data(); }
};

enum class TriggerKind {
	OnCreate,
	NamedEvent,
	KeyPressed,
	KeyReleased,
	KeyHeld,
	MousePressed,
	MouseReleased,
	MouseHeld,
	OverlapStart,
	OverlapStop,
	CollisionStart,
	CollisionStop
};

enum class ReentryMode {
	IgnoreWhileRunning,
	Restart,
	Queue,
	Parallel
};

enum class SequenceItemKind {
	Item,
	TimedItem,
	Wait,
	EmitEvent
};

enum class Ease {
	Linear,
	InQuad,
	OutQuad,
	InOutQuad,
	OutCubic,
	OutBack
};

enum class SpawnOrigin {
	OwnerEntity,
	Position
};

enum class SpawnArea {
	Point,
	Rectangle,
	Circle
};

enum class ComponentKind {
	Transform,
	Visible,
	Sprite,
	Collider,
	Health,
	Zombie,
	Damage,
	Lifetime
};

enum class RegisteredSequenceItemKind {
	SetVisible,
	MoveTo,
	RotateTo,
	PlayAudio,
	SetColliderMode,
	ApplyDamage,
	SpawnEntity,
	AddComponent,
	RemoveComponent
};

enum class LifecycleEventKind {
	Start,
	// Progress,
	Complete,
	Reset,
	Stop,
	Pause,
	Resume,
	PointStart,
	PointComplete,
	Repeat,
	Yoyo
};

enum class LifecycleCallbackKind {
	Item,
	EmitEvent
};

constexpr std::array kTriggerNames{ "On Create",	  "Named Event",			 "Key Pressed",
									"Key Released",	  "Key Held",		 "Mouse Pressed",
									"Mouse Released", "Mouse Held",		 "Overlap Start",
									"Overlap Stop",	  "Collision Start", "Collision Stop" };
constexpr std::array kReentryNames{ "Ignore", "Restart", "Queue", "Parallel" };
constexpr std::array kSequenceItemNames{
	"Item", "Timed Item", "Wait", "Emit Event"
};
constexpr std::array kEaseNames{
	"Linear", "In Quad", "Out Quad", "In-Out Quad", "Out Cubic", "Out Back"
};
constexpr std::array kSpawnOriginNames{ "Entity", "Position" };
constexpr std::array kSpawnAreaNames{ "Point", "Rectangle", "Circle" };
constexpr std::array kColliderModeNames{
	"None", "Overlap", "Discrete", "Continuous"
};
constexpr std::array kLifecycleEventNames{ "On Start",		/*"On Progress",*/ "On Complete",
										   "On Reset",		"On Stop",
										   "On Pause",		"On Resume",
										   "On Item Start", "On Item Complete",
										   "On Repeat",		"On Yoyo" };
constexpr std::array kLifecycleCallbackKindNames{ "Item", "Emit Event" };

template <typename TEnum, std::size_t N>
bool DrawEnumCombo(
	const char* label,
	TEnum& value,
	const std::array<const char*, N>& names,
	float width = -FLT_MIN
) {
	if (width != 0.0f) {
		ImGui::SetNextItemWidth(width);
	}

	bool changed{ false };
	const int current{ static_cast<int>(value) };

	if (ImGui::BeginCombo(label, names[static_cast<std::size_t>(current)])) {
		for (int i{ 0 }; i < static_cast<int>(N); ++i) {
			const bool selected{ i == current };
			if (ImGui::Selectable(names[static_cast<std::size_t>(i)], selected)) {
				value = static_cast<TEnum>(i);
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	return changed;
}

bool DrawSpawnOriginCombo(const char* label, SpawnOrigin& origin, float width = -FLT_MIN) {
	if (width != 0.0f) {
		ImGui::SetNextItemWidth(width);
	}

	const auto tooltip_for = [](SpawnOrigin value) {
		switch (value) {
			case SpawnOrigin::OwnerEntity:
				return "Use the sequence entity as the placement origin. X and Y are offsets.";
			case SpawnOrigin::Position:
				return "Use an explicit world-space position. X and Y are coordinates.";
		}
		return "";
	};

	bool changed{ false };
	const char* preview{ kSpawnOriginNames[static_cast<std::size_t>(origin)] };

	if (ImGui::BeginCombo(label, preview)) {
		for (int i{ 0 }; i < static_cast<int>(kSpawnOriginNames.size()); ++i) {
			const auto candidate{ static_cast<SpawnOrigin>(i) };
			const bool selected{ origin == candidate };

			if (ImGui::Selectable(kSpawnOriginNames[static_cast<std::size_t>(i)], selected)) {
				origin	= candidate;
				changed = true;
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", tooltip_for(candidate));
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip_for(origin));
	}

	return changed;
}

bool DrawTriggerKindCombo(const char* label, TriggerKind& kind, float width = -FLT_MIN) {
	ImGui::SetNextItemWidth(width);

	const char* preview{ kTriggerNames[static_cast<std::size_t>(kind)] };
	bool changed{ false };

	if (!ImGui::BeginCombo(label, preview)) {
		return false;
	}

	auto draw_item = [&](TriggerKind candidate) {
		const bool selected{ kind == candidate };

		if (ImGui::MenuItem(
				kTriggerNames[static_cast<std::size_t>(candidate)], nullptr, selected
			)) {
			kind	= candidate;
			changed = true;
		}
	};

	draw_item(TriggerKind::OnCreate);
	draw_item(TriggerKind::NamedEvent);
	ImGui::Separator();

	if (ImGui::BeginMenu("Key")) {
		draw_item(TriggerKind::KeyPressed);
		draw_item(TriggerKind::KeyReleased);
		draw_item(TriggerKind::KeyHeld);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Mouse")) {
		draw_item(TriggerKind::MousePressed);
		draw_item(TriggerKind::MouseReleased);
		draw_item(TriggerKind::MouseHeld);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Overlap")) {
		draw_item(TriggerKind::OverlapStart);
		draw_item(TriggerKind::OverlapStop);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Collision")) {
		draw_item(TriggerKind::CollisionStart);
		draw_item(TriggerKind::CollisionStop);
		ImGui::EndMenu();
	}

	ImGui::EndCombo();
	return changed;
}

bool DrawLifecycleEventCombo(const char* label, LifecycleEventKind& event, float width = -FLT_MIN) {
	ImGui::SetNextItemWidth(width);

	const char* preview{ kLifecycleEventNames[static_cast<std::size_t>(event)] };
	bool changed{ false };

	if (!ImGui::BeginCombo(label, preview)) {
		return false;
	}

	auto draw_item = [&](LifecycleEventKind candidate) {
		const bool selected{ event == candidate };

		if (ImGui::MenuItem(
				kLifecycleEventNames[static_cast<std::size_t>(candidate)], nullptr, selected
			)) {
			event	= candidate;
			changed = true;
		}
	};

	draw_item(LifecycleEventKind::Start);
	// draw_item(LifecycleEventKind::Progress);
	draw_item(LifecycleEventKind::Complete);
	draw_item(LifecycleEventKind::Reset);
	draw_item(LifecycleEventKind::Stop);
	draw_item(LifecycleEventKind::Pause);
	draw_item(LifecycleEventKind::Resume);

	ImGui::Separator();

	draw_item(LifecycleEventKind::PointStart);
	draw_item(LifecycleEventKind::PointComplete);

	ImGui::Separator();

	draw_item(LifecycleEventKind::Repeat);
	draw_item(LifecycleEventKind::Yoyo);

	ImGui::EndCombo();
	return changed;
}

void DrawItemTooltip(const char* text) {
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

std::string BuildSelectionTooltip(
	std::string_view heading, const std::vector<std::string_view>& selected,
	std::string_view empty_item = "None"
);

std::string BuildCommaSeparatedTooltip(
	std::string_view explanation, std::string_view comma_separated_values
);

std::optional<float> ParseDurationMilliseconds(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
		text.remove_prefix(1);
	}

	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
		text.remove_suffix(1);
	}

	if (text.empty()) {
		return std::nullopt;
	}

	std::string input{ text };
	char* end{ nullptr };
	const double value{ std::strtod(input.c_str(), &end) };

	if (end == input.c_str() || !std::isfinite(value) || value < 0.0) {
		return std::nullopt;
	}

	std::string_view unit{ end };

	while (!unit.empty() && std::isspace(static_cast<unsigned char>(unit.front()))) {
		unit.remove_prefix(1);
	}

	while (!unit.empty() && std::isspace(static_cast<unsigned char>(unit.back()))) {
		unit.remove_suffix(1);
	}

	if (unit.empty()) {
		unit = "ms";
	}

	std::string normalized_unit{ unit };
	std::ranges::transform(normalized_unit, normalized_unit.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	double multiplier_ms{ 0.0 };

	if (normalized_unit == "ns") {
		multiplier_ms = 0.000001;
	} else if (normalized_unit == "us" || normalized_unit == "µs" ||
			   normalized_unit == "μs") {
		multiplier_ms = 0.001;
	} else if (normalized_unit == "ms") {
		multiplier_ms = 1.0;
	} else if (normalized_unit == "s" || normalized_unit == "sec") {
		multiplier_ms = 1000.0;
	} else if (normalized_unit == "m" || normalized_unit == "min") {
		multiplier_ms = 60.0 * 1000.0;
	} else if (normalized_unit == "h" || normalized_unit == "hr") {
		multiplier_ms = 60.0 * 60.0 * 1000.0;
	} else if (normalized_unit == "d" || normalized_unit == "day") {
		multiplier_ms = 24.0 * 60.0 * 60.0 * 1000.0;
	} else {
		return std::nullopt;
	}

	const double milliseconds{ value * multiplier_ms };

	if (!std::isfinite(milliseconds) || milliseconds > static_cast<double>(FLT_MAX)) {
		return std::nullopt;
	}

	return static_cast<float>(milliseconds);
}

void FormatDurationMilliseconds(float milliseconds, char* buffer, std::size_t size) {
	const double clamped{ std::max(0.0, static_cast<double>(milliseconds)) };

	struct Unit {
		double milliseconds;
		const char* suffix;
	};

	constexpr std::array units{
		Unit{ 24.0 * 60.0 * 60.0 * 1000.0, "d" },
		Unit{ 60.0 * 60.0 * 1000.0, "h" },
		Unit{ 60.0 * 1000.0, "m" },
		Unit{ 1000.0, "s" },
		Unit{ 1.0, "ms" },
		Unit{ 0.001, "us" },
		Unit{ 0.000001, "ns" },
	};

	if (clamped == 0.0) {
		std::snprintf(buffer, size, "0ms");
		return;
	}

	for (const auto& unit : units) {
		if (clamped < unit.milliseconds && unit.milliseconds != units.back().milliseconds) {
			continue;
		}

		std::snprintf(buffer, size, "%.4g%s", clamped / unit.milliseconds, unit.suffix);
		return;
	}
}

struct DurationEditState {
	std::array<char, 32> buffer{};
	bool initialized{ false };
	bool was_active{ false };
};

bool DrawDurationInput(const char* label, float& milliseconds, float width, const char* tooltip) {
	static std::unordered_map<ImGuiID, DurationEditState> states;

	const ImGuiID id{ ImGui::GetID(label) };
	auto& state{ states[id] };

	if (!state.initialized || !state.was_active) {
		FormatDurationMilliseconds(milliseconds, state.buffer.data(), state.buffer.size());
		state.initialized = true;
	}

	ImGui::SetNextItemWidth(width);

	const bool submitted{ ImGui::InputText(
		label, state.buffer.data(), state.buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue
	) };

	const bool active{ ImGui::IsItemActive() };
	const bool commit{ submitted || ImGui::IsItemDeactivatedAfterEdit() };

	if (commit) {
		if (const auto parsed{ ParseDurationMilliseconds(state.buffer.data()) }) {
			milliseconds = *parsed;
		} else {
			milliseconds = 1000.0f;
		}

		FormatDurationMilliseconds(milliseconds, state.buffer.data(), state.buffer.size());
	}

	state.was_active = active;

	if (tooltip) {
		DrawItemTooltip(tooltip);
	}

	return commit;
}

void OpenPopupOnRightClick(const char* popup_name) {
	if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
		ImGui::OpenPopup(popup_name);
	}
}

void DrawVolumeControl(float& volume) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::SliderFloat("##Volume", &volume, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

	DrawItemTooltip("Audio volume. Double-click to enter an exact value.");

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		ImGui::OpenPopup("ExactVolume");
	}

	if (ImGui::BeginPopup("ExactVolume")) {
		ImGui::SetNextItemWidth(110.0f);
		ImGui::InputFloat("Volume", &volume, 0.01f, 0.1f, "%.3f");
		volume = std::clamp(volume, 0.0f, 1.0f);
		ImGui::EndPopup();
	}
}

float GetCountControlWidth(const char* label) {
	constexpr float button_width{ 22.0f };
	constexpr float text_button_spacing{ 3.0f };
	constexpr float button_spacing{ 3.0f };
	const std::string widest_text{ std::string{ label } + ": 100" };
	return ImGui::CalcTextSize(widest_text.c_str()).x + text_button_spacing + button_width * 2.0f +
		   button_spacing;
}

void DrawCountControl(
	const char* label, int& value, int minimum, int maximum = 100, bool disabled = false,
	const char* tooltip = nullptr
) {
	if (minimum > maximum) {
		std::swap(minimum, maximum);
	}
	value = std::clamp(value, minimum, maximum);

	constexpr float button_width{ 22.0f };
	constexpr float text_button_spacing{ 3.0f };
	constexpr float button_spacing{ 3.0f };
	const std::string widest_text{ std::string{ label } + ": 100" };
	const float reserved_text_width{ ImGui::CalcTextSize(widest_text.c_str()).x };
	const float text_start_x{ ImGui::GetCursorScreenPos().x };

	ImGui::PushID(label);
	ImGui::BeginDisabled(disabled);

	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s: %d", label, value);
	if (tooltip) {
		DrawItemTooltip(tooltip);
	}

	ImGui::SameLine();
	ImGui::SetCursorScreenPos(
		ImVec2{ text_start_x + reserved_text_width + text_button_spacing,
				ImGui::GetCursorScreenPos().y }
	);
	ImGui::BeginDisabled(value >= maximum);
	if (ImGui::Button("+", ImVec2{ button_width, 0.0f })) {
		++value;
	}
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, button_spacing);
	ImGui::BeginDisabled(value <= minimum);
	if (ImGui::Button("-", ImVec2{ button_width, 0.0f })) {
		--value;
	}
	ImGui::EndDisabled();

	ImGui::EndDisabled();
	ImGui::PopID();
}

struct TriggerDefinition {
	Id id{ NextId() };
	TriggerKind kind{ TriggerKind::OnCreate };
	bool enabled{ true };
	TextBuffer<48> key{ "Space" };
	TextBuffer<32> mouse_button{ "Left" };
	TextBuffer<96> tag_filter{ "Player" };
	TextBuffer<64> mask_filter{};
	TextBuffer<80> event_name{ "door.opened" };
	float duration_ms{ 0.0f };
};

struct TransformComponentData {
	float position[2]{ 0.0f, 0.0f };
	float rotation{ 0.0f };
	float scale[2]{ 1.0f, 1.0f };
};

struct VisibleComponentData { bool visible{ true }; };

struct SpriteComponentData {
	TextBuffer<96> texture{ "textures/entity.png" };
	float size[2]{ 32.0f, 32.0f };
	bool flip_x{ false };
};

struct ColliderComponentData {
	int mode{ 1 };
	float radius{ 16.0f };
	int mask{ 1 };
};

struct HealthComponentData {
	float maximum{ 100.0f };
	float current{ 100.0f };
};

struct ZombieComponentData {};

struct DamageComponentData {
	float amount{ 10.0f };
	TextBuffer<48> damage_type{ "Physical" };
};

struct LifetimeComponentData { float duration_ms{ 3000.0f }; };

using ComponentData = std::variant<
	TransformComponentData,
	VisibleComponentData,
	SpriteComponentData,
	ColliderComponentData,
	HealthComponentData,
	ZombieComponentData,
	DamageComponentData,
	LifetimeComponentData
>;

struct ComponentDefinition {
	Id id{ NextId() };
	ComponentKind kind{ ComponentKind::Transform };
	ComponentData data{ TransformComponentData{} };
};

struct ComponentDescriptor {
	ComponentKind kind;
	const char* key;
	const char* label;
	const char* group;
	const char* description;
};

constexpr std::array kComponentRegistry{
	ComponentDescriptor{ ComponentKind::Transform, "ptgn.Transform", "Transform", "Core", "Position, rotation, and scale." },
	ComponentDescriptor{ ComponentKind::Visible, "ptgn.Visible", "Visible", "Core", "Controls whether the entity is visible." },
	ComponentDescriptor{ ComponentKind::Sprite, "ptgn.Sprite", "Sprite", "Graphics", "Sprite texture and display size." },
	ComponentDescriptor{ ComponentKind::Collider, "ptgn.Collider", "Collider", "Physics", "Collider mode, shape radius, and mask." },
	ComponentDescriptor{ ComponentKind::Health, "game.Health", "Health", "Gameplay", "Maximum and current health." },
	ComponentDescriptor{ ComponentKind::Zombie, "game.Zombie", "Zombie", "Gameplay", "Tag component identifying zombie entities." },
	ComponentDescriptor{ ComponentKind::Damage, "game.Damage", "Damage", "Gameplay", "Damage amount and type." },
	ComponentDescriptor{ ComponentKind::Lifetime, "game.Lifetime", "Lifetime", "Gameplay", "Destroys the runtime entity after a duration." },
};

const ComponentDescriptor& GetComponentDescriptor(ComponentKind kind) {
	const auto it{ std::ranges::find_if(kComponentRegistry, [kind](const auto& descriptor) {
		return descriptor.kind == kind;
	}) };
	return it != kComponentRegistry.end() ? *it : kComponentRegistry.front();
}

ComponentDefinition MakeComponent(ComponentKind kind) {
	ComponentDefinition component;
	component.kind = kind;

	switch (kind) {
		case ComponentKind::Transform: component.data = TransformComponentData{}; break;
		case ComponentKind::Visible: component.data = VisibleComponentData{}; break;
		case ComponentKind::Sprite: component.data = SpriteComponentData{}; break;
		case ComponentKind::Collider: component.data = ColliderComponentData{}; break;
		case ComponentKind::Health: component.data = HealthComponentData{}; break;
		case ComponentKind::Zombie: component.data = ZombieComponentData{}; break;
		case ComponentKind::Damage: component.data = DamageComponentData{}; break;
		case ComponentKind::Lifetime: component.data = LifetimeComponentData{}; break;
	}

	return component;
}

std::string ComponentSelectionPreview(
	const std::vector<ComponentKind>& components, std::string_view empty_text
) {
	if (components.empty()) {
		return std::string{ empty_text };
	}

	std::string preview;
	for (const auto kind : components) {
		if (!preview.empty()) {
			preview += ", ";
		}
		preview += GetComponentDescriptor(kind).label;
	}

	return preview;
}

struct PrefabDefinition {
	Id id{ NextId() };
	TextBuffer<96> key{ "prefabs/new_entity" };
	TextBuffer<80> name{ "New Prefab" };
	TextBuffer<64> tag{};
	std::vector<ComponentDefinition> components;
};

struct PrefabRegistry {
	std::vector<PrefabDefinition> definitions;

	PrefabDefinition* FindByKey(std::string_view key) {
		const auto it{ std::ranges::find_if(definitions, [key](const auto& prefab) {
			return prefab.key.View() == key;
		}) };
		return it != definitions.end() ? &*it : nullptr;
	}

	const PrefabDefinition* FindByKey(std::string_view key) const {
		return const_cast<PrefabRegistry*>(this)->FindByKey(key);
	}
};

struct SetVisibleParams { bool visible{ true }; };
struct MoveToParams { TextBuffer<64> target_tag{ "Self" }; float destination[2]{ 0.0f, 64.0f }; bool relative{ true }; };
struct RotateToParams { float degrees{ 90.0f }; bool shortest_path{ true };
};

struct PlayAudioParams {
	TextBuffer<80> asset{ "door_open" };
	float volume{ 1.0f };
	int loops{ 0 };
};
struct SetColliderModeParams { int mode{ 0 }; };
struct ApplyDamageParams { float amount{ 10.0f }; TextBuffer<48> damage_type{ "Physical" }; bool critical{ false }; };

struct SpawnEntityParams {
	TextBuffer<96> prefab_key{ "prefabs/zombie" };
	int count{ 1 };
	SpawnOrigin origin{ SpawnOrigin::OwnerEntity };
	SpawnArea area{ SpawnArea::Point };
	float center[2]{ 0.0f, 0.0f };
	float rectangle_size[2]{ 128.0f, 128.0f };
	float radius{ 64.0f };
	bool parent_to_owner{ false };
	bool inherit_owner_rotation{ true };
	bool inherit_owner_scale{ true };
	bool random_rotation{ false };
};

struct AddComponentParams {
	std::vector<ComponentDefinition> components{ MakeComponent(ComponentKind::Health) };
};

struct RemoveComponentParams {
	std::vector<ComponentKind> components{ ComponentKind::Health };
};

using RegisteredSequenceItemParameters = std::variant<
	SetVisibleParams, MoveToParams, RotateToParams, PlayAudioParams, SetColliderModeParams,
	ApplyDamageParams, SpawnEntityParams, AddComponentParams, RemoveComponentParams>;

struct RegisteredSequenceItem {
	RegisteredSequenceItemKind kind{ RegisteredSequenceItemKind::SetVisible };
	RegisteredSequenceItemParameters parameters{ SetVisibleParams{} };
};

struct RegisteredSequenceItemDescriptor {
	RegisteredSequenceItemKind kind;
	const char* key;
	const char* label;
	const char* group;
	const char* description;
	bool supports_timed;
};

constexpr std::array kRegisteredSequenceItemRegistry{
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::SpawnEntity, "engine.spawn_entity", "Spawn Entity", "Entity",
					  "Spawns one or more instances of one prefab with configurable placement.",
					  false },
	RegisteredSequenceItemDescriptor{
		RegisteredSequenceItemKind::AddComponent, "engine.add_component", "Add Components", "Entity",
		"Adds one or more registered components with reflected serialized values to the owner.",
		false },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::RemoveComponent, "engine.remove_component", "Remove Components",
					  "Entity",
					  "Removes one or more selected registered components from the owner.", false },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::SetVisible, "engine.set_visible", "Set Visible", "Entity",
					  "Changes entity visibility.", false },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::MoveTo, "engine.move_to", "Move To", "Transform",
					  "Moves an entity to a target position.", true },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::RotateTo, "engine.rotate_to", "Rotate To", "Transform",
					  "Rotates an entity to a target angle.", true },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::PlayAudio, "engine.play_audio", "Play Audio", "Audio",
					  "Plays an audio asset.", false },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::SetColliderMode, "engine.set_collider_mode", "Set Collider Mode",
					  "Physics", "Changes the collider mode.", false },
	RegisteredSequenceItemDescriptor{ RegisteredSequenceItemKind::ApplyDamage, "game.apply_damage", "Apply Damage", "Game",
					  "Example user-registered action.", false },
};

const RegisteredSequenceItemDescriptor& GetRegisteredSequenceItemDescriptor(RegisteredSequenceItemKind kind) {
	const auto it{ std::ranges::find_if(kRegisteredSequenceItemRegistry, [kind](const auto& descriptor) {
		return descriptor.kind == kind;
	}) };
	return it != kRegisteredSequenceItemRegistry.end() ? *it : kRegisteredSequenceItemRegistry.front();
}

RegisteredSequenceItem MakeRegisteredSequenceItem(RegisteredSequenceItemKind kind) {
	RegisteredSequenceItem action;
	action.kind = kind;

	switch (kind) {
		case RegisteredSequenceItemKind::SetVisible: action.parameters = SetVisibleParams{}; break;
		case RegisteredSequenceItemKind::MoveTo: action.parameters = MoveToParams{}; break;
		case RegisteredSequenceItemKind::RotateTo: action.parameters = RotateToParams{}; break;
		case RegisteredSequenceItemKind::PlayAudio: action.parameters = PlayAudioParams{}; break;
		case RegisteredSequenceItemKind::SetColliderMode: action.parameters = SetColliderModeParams{}; break;
		case RegisteredSequenceItemKind::ApplyDamage: action.parameters = ApplyDamageParams{}; break;
		case RegisteredSequenceItemKind::SpawnEntity:	  action.parameters = SpawnEntityParams{}; break;
		case RegisteredSequenceItemKind::AddComponent: action.parameters = AddComponentParams{}; break;
		case RegisteredSequenceItemKind::RemoveComponent: action.parameters = RemoveComponentParams{}; break;
	}

	return action;
}

struct LifecycleCallbackDefinition {
	Id id{ NextId() };
	bool enabled{ true };
	LifecycleEventKind event{ LifecycleEventKind::Complete };
	LifecycleCallbackKind kind{ LifecycleCallbackKind::EmitEvent };
	RegisteredSequenceItem registered_item{ MakeRegisteredSequenceItem(RegisteredSequenceItemKind::SetVisible) };
	TextBuffer<80> event_name{ "sequence.completed" };
};

struct InstantSequenceItem { RegisteredSequenceItem registered_item; };
struct TimedSequenceItem {
	RegisteredSequenceItem registered_item{ MakeRegisteredSequenceItem(RegisteredSequenceItemKind::MoveTo) };
	float duration_ms{ 300.0f };
	Ease ease{ Ease::Linear };
	int additional_repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };
};

std::string TimedFlagsPreview(const TimedSequenceItem& timed) {
	std::string preview;

	auto append = [&preview](std::string_view value) {
		if (!preview.empty()) {
			preview += ", ";
		}
		preview += value;
	};

	if (timed.infinite_repeats) {
		append("Infinite");
	}
	if (timed.reversed) {
		append("Reversed");
	}
	if (timed.yoyo) {
		append("Yoyo");
	}

	return preview.empty() ? "Options" : preview;
}

void DrawTimedFlagsCombo(TimedSequenceItem& timed) {
	const std::string preview{ TimedFlagsPreview(timed) };
	ImGui::SetNextItemWidth(-FLT_MIN);
	const bool open{ ImGui::BeginCombo("##TimedFlags", preview.c_str()) };

	if (ImGui::IsItemHovered()) {
		std::vector<std::string_view> selected;
		if (timed.infinite_repeats) {
			selected.emplace_back("Infinite");
		}
		if (timed.reversed) {
			selected.emplace_back("Reversed");
		}
		if (timed.yoyo) {
			selected.emplace_back("Yoyo");
		}
		const std::string tooltip{ BuildSelectionTooltip("Selected options:", selected) };
		ImGui::SetTooltip("%s", tooltip.c_str());
	}

	if (open) {
		ImGui::Checkbox("Infinite", &timed.infinite_repeats);
		DrawItemTooltip("Repeat the timed action indefinitely.");
		ImGui::Checkbox("Reversed", &timed.reversed);
		ImGui::Checkbox("Yoyo", &timed.yoyo);
		ImGui::EndCombo();
	}
}
struct WaitItem { float duration_ms{ 250.0f }; };
struct EmitEventItem { TextBuffer<80> event_name{ "door.opened" }; };

using SequenceItemData = std::variant<InstantSequenceItem, TimedSequenceItem, WaitItem, EmitEventItem>;

struct SequenceItem {
	Id id{ NextId() };
	bool enabled{ true };
	SequenceItemKind kind{ SequenceItemKind::Item };
	SequenceItemData data{ InstantSequenceItem{} };
};

SequenceItem MakeSequenceItem(SequenceItemKind kind) {
	SequenceItem item;
	item.kind = kind;

	switch (kind) {
		case SequenceItemKind::Item: item.data = InstantSequenceItem{}; break;
		case SequenceItemKind::TimedItem: item.data = TimedSequenceItem{}; break;
		case SequenceItemKind::Wait: item.data = WaitItem{}; break;
		case SequenceItemKind::EmitEvent: item.data = EmitEventItem{}; break;
	}

	return item;
}

void SetSequenceItemKind(SequenceItem& item, SequenceItemKind kind) {
	if (item.kind == kind) {
		return;
	}
	const Id id{ item.id };
	const bool enabled{ item.enabled };
	item = MakeSequenceItem(kind);
	item.id = id;
	item.enabled = enabled;
}

struct ScriptSequenceDefinition {
	Id id{ NextId() };
	TextBuffer<80> name{ "New Script Sequence" };
	ReentryMode reentry{ ReentryMode::IgnoreWhileRunning };
	bool destroy_on_complete{ false };
	std::vector<TriggerDefinition> triggers;
	std::vector<TriggerDefinition> stop_triggers;
	std::vector<SequenceItem> sequence;
	std::vector<LifecycleCallbackDefinition> lifecycle_callbacks;
};

struct RuntimeState {
	bool running{ false };
	bool paused{ false };
	bool completed{ false };
	bool queued{ false };

	std::size_t item_index{ 0 };
	std::optional<std::size_t> started_item_index;

	float elapsed_ms{ 0.0f };
	int current_repeat{ 0 };
	bool currently_reversed{ false };

	Id active_target_entity{ 0 };
	float item_start[2]{ 0.0f, 0.0f };
	float item_end[2]{ 0.0f, 0.0f };
	float start_rotation{ 0.0f };
	float end_rotation{ 0.0f };

	int completed_runs{ 0 };
};

struct ScriptSequence {
	Id id{ NextId() };
	bool enabled{ true };
	bool global_reference{ false };
	Id global_sequence_id{ 0 };
	ScriptSequenceDefinition local_definition;
	RuntimeState runtime;
};

struct ScriptsComponent { std::vector<ScriptSequence> sequences; };

struct DemoRectangle {
	float size[2]{ 96.0f, 36.0f };
	ImU32 color{ IM_COL32(90, 110, 145, 255) };
	bool sensor{ false };
};

struct DemoPlayer {
	float speed{ 220.0f };
};

struct DemoDoorSensor {};
struct DemoMovingPanel {};

struct EntityData {
	ptgn::Entity entity;
	Id id{ NextId() };
	TextBuffer<64> name{ "Entity" };
	TextBuffer<64> tag{};
	float position[2]{ 0.0f, 0.0f };
	float rotation{ 0.0f };
	float scale[2]{ 1.0f, 1.0f };
	bool visible{ true };
};

EntityData CreateEntityData(ptgn::Manager& manager) {
	EntityData data;
	data.entity = ptgn::Entity{ manager.CreateEntity() };
	data.entity.Add<DemoRectangle>();
	return data;
}

ScriptsComponent* TryGetScripts(EntityData& entity) {
	return entity.entity.TryGet<ScriptsComponent>();
}

const ScriptsComponent* TryGetScripts(const EntityData& entity) {
	return entity.entity.TryGet<ScriptsComponent>();
}

struct GlobalScriptSequenceRegistry {
	std::vector<ScriptSequenceDefinition> definitions;

	ScriptSequenceDefinition* Find(Id id) {
		const auto it{ std::ranges::find_if(definitions, [id](const auto& definition) {
			return definition.id == id;
		}) };
		return it != definitions.end() ? &*it : nullptr;
	}

	const ScriptSequenceDefinition* Find(Id id) const {
		return const_cast<GlobalScriptSequenceRegistry*>(this)->Find(id);
	}

	ScriptSequenceDefinition* FindByName(std::string_view name) {
		const auto it{ std::ranges::find_if(definitions, [name](const auto& definition) {
			return definition.name.View() == name;
		}) };
		return it != definitions.end() ? &*it : nullptr;
	}

	const ScriptSequenceDefinition* FindByName(std::string_view name) const {
		return const_cast<GlobalScriptSequenceRegistry*>(this)->FindByName(name);
	}
};

ScriptSequenceDefinition* ResolveScriptSequence(ScriptSequence& binding, GlobalScriptSequenceRegistry& registry) {
	return binding.global_reference ? registry.Find(binding.global_sequence_id) : &binding.local_definition;
}

const ScriptSequenceDefinition* ResolveScriptSequence(const ScriptSequence& binding, const GlobalScriptSequenceRegistry& registry) {
	return binding.global_reference ? registry.Find(binding.global_sequence_id) : &binding.local_definition;
}

ScriptSequenceDefinition CloneScriptSequenceDefinition(const ScriptSequenceDefinition& source) {
	ScriptSequenceDefinition copy{ source };
	copy.id = NextId();
	for (auto& trigger : copy.triggers) {
		trigger.id = NextId();
	}
	for (auto& trigger : copy.stop_triggers) {
		trigger.id = NextId();
	}
	for (auto& item : copy.sequence) {
		item.id = NextId();
	}
	for (auto& callback : copy.lifecycle_callbacks) {
		callback.id = NextId();
	}
	return copy;
}

TriggerDefinition MakeNamedEventTrigger(std::string_view event_name) {
	TriggerDefinition trigger;
	trigger.kind = TriggerKind::NamedEvent;
	trigger.event_name.Assign(event_name);
	return trigger;
}

TriggerDefinition MakeOverlapTrigger(TriggerKind kind, std::string_view other_tag) {
	TriggerDefinition trigger;
	trigger.kind = kind;
	trigger.tag_filter.Assign(other_tag);
	return trigger;
}

RegisteredSequenceItem MakeMoveToItem(
	std::string_view target_tag,
	float x,
	float y,
	bool relative = false
) {
	auto item{ MakeRegisteredSequenceItem(RegisteredSequenceItemKind::MoveTo) };
	auto& params{ std::get<MoveToParams>(item.parameters) };
	params.target_tag.Assign(target_tag);
	params.destination[0] = x;
	params.destination[1] = y;
	params.relative = relative;
	return item;
}

class ScriptSequenceBuilder;

class TimedSequenceItemBuilder {
public:
	TimedSequenceItemBuilder& EaseWith(Ease ease);
	TimedSequenceItemBuilder& Repeat(int additional_repeats);
	TimedSequenceItemBuilder& Infinite();
	TimedSequenceItemBuilder& Reversed(bool reversed = true);
	TimedSequenceItemBuilder& Yoyo(bool yoyo = true);

	ScriptSequenceBuilder& End();

private:
	friend class ScriptSequenceBuilder;

	TimedSequenceItemBuilder(ScriptSequenceBuilder& parent, Id item_id) :
		parent_{ &parent },
		item_id_{ item_id } {}

	TimedSequenceItem& Item();

	ScriptSequenceBuilder* parent_{ nullptr };
	Id item_id_{ 0 };
};

class ScriptSequenceBuilder {
public:
	explicit ScriptSequenceBuilder(std::string_view name) {
		definition_.name.Assign(name);
		definition_.triggers.clear();
	}

	ScriptSequenceBuilder& Reentry(ReentryMode mode) {
		definition_.reentry = mode;
		return *this;
	}

	ScriptSequenceBuilder& StartOn(TriggerDefinition trigger) {
		definition_.triggers.push_back(std::move(trigger));
		return *this;
	}

	ScriptSequenceBuilder& StopOn(TriggerDefinition trigger) {
		definition_.stop_triggers.push_back(std::move(trigger));
		return *this;
	}

	ScriptSequenceBuilder& Then(RegisteredSequenceItem registered_item) {
		auto item{ MakeSequenceItem(SequenceItemKind::Item) };
		std::get<InstantSequenceItem>(item.data).registered_item = std::move(registered_item);
		definition_.sequence.push_back(std::move(item));
		return *this;
	}

	TimedSequenceItemBuilder During(
		float duration_ms,
		RegisteredSequenceItem registered_item
	) {
		auto item{ MakeSequenceItem(SequenceItemKind::TimedItem) };
		auto& timed{ std::get<TimedSequenceItem>(item.data) };
		timed.registered_item = std::move(registered_item);
		timed.duration_ms = std::max(0.0f, duration_ms);
		const Id item_id{ item.id };
		definition_.sequence.push_back(std::move(item));
		return TimedSequenceItemBuilder{ *this, item_id };
	}

	ScriptSequenceBuilder& Wait(float duration_ms) {
		auto item{ MakeSequenceItem(SequenceItemKind::Wait) };
		std::get<WaitItem>(item.data).duration_ms = std::max(0.0f, duration_ms);
		definition_.sequence.push_back(std::move(item));
		return *this;
	}

	ScriptSequenceBuilder& EmitEvent(std::string_view event_name) {
		auto item{ MakeSequenceItem(SequenceItemKind::EmitEvent) };
		std::get<EmitEventItem>(item.data).event_name.Assign(event_name);
		definition_.sequence.push_back(std::move(item));
		return *this;
	}

	ScriptSequenceDefinition BuildDefinition() {
		return std::move(definition_);
	}

	ScriptSequence Build() {
		ScriptSequence sequence;
		sequence.local_definition = std::move(definition_);
		return sequence;
	}

private:
	friend class TimedSequenceItemBuilder;

	TimedSequenceItem& FindTimedItem(Id id) {
		auto it{ std::ranges::find_if(definition_.sequence, [id](const SequenceItem& item) {
			return item.id == id;
		}) };

		if (it == definition_.sequence.end() || it->kind != SequenceItemKind::TimedItem) {
			std::abort();
		}

		return std::get<TimedSequenceItem>(it->data);
	}

	ScriptSequenceDefinition definition_;
};

TimedSequenceItem& TimedSequenceItemBuilder::Item() {
	return parent_->FindTimedItem(item_id_);
}

TimedSequenceItemBuilder& TimedSequenceItemBuilder::EaseWith(Ease ease) {
	Item().ease = ease;
	return *this;
}

TimedSequenceItemBuilder& TimedSequenceItemBuilder::Repeat(int additional_repeats) {
	Item().additional_repeats = std::max(0, additional_repeats);
	return *this;
}

TimedSequenceItemBuilder& TimedSequenceItemBuilder::Infinite() {
	Item().infinite_repeats = true;
	return *this;
}

TimedSequenceItemBuilder& TimedSequenceItemBuilder::Reversed(bool reversed) {
	Item().reversed = reversed;
	return *this;
}

TimedSequenceItemBuilder& TimedSequenceItemBuilder::Yoyo(bool yoyo) {
	Item().yoyo = yoyo;
	return *this;
}

ScriptSequenceBuilder& TimedSequenceItemBuilder::End() {
	return *parent_;
}

struct QueuedEvent {
	TriggerKind kind{ TriggerKind::NamedEvent };
	std::string name;
	Id target_entity{ 0 };
	Id source_entity{ 0 };
	Id other_entity{ 0 };
};

struct ActivityEntry {
	std::string text;
};

struct DemoRuntimeContext {
	std::deque<QueuedEvent> pending_events;
	std::vector<ActivityEntry> activity;
	std::vector<EntityData>* entities{ nullptr };
	bool player_overlapping_door{ false };
};

void AddActivity(DemoRuntimeContext& context, std::string text) {
	context.activity.push_back({ std::move(text) });
	constexpr std::size_t kMaxEntries{ 100 };

	if (context.activity.size() > kMaxEntries) {
		context.activity.erase(
			context.activity.begin(),
			context.activity.begin() +
				static_cast<std::ptrdiff_t>(context.activity.size() - kMaxEntries)
		);
	}
}

template <typename T>
void MoveItem(std::vector<T>& items, int from, int to) {
	if (from < 0 || to < 0 || from >= static_cast<int>(items.size()) ||
		to >= static_cast<int>(items.size()) || from == to) {
		return;
	}
	T moved{ std::move(items[static_cast<std::size_t>(from)]) };
	items.erase(items.begin() + from);
	items.insert(items.begin() + to, std::move(moved));
}

EntityData* FindEntityById(std::vector<EntityData>& entities, Id id) {
	const auto it{ std::ranges::find_if(entities, [id](const EntityData& entity) {
		return entity.id == id;
	}) };
	return it != entities.end() ? &*it : nullptr;
}

const EntityData* FindEntityById(const std::vector<EntityData>& entities, Id id) {
	return const_cast<std::vector<EntityData>&>(entities).empty()
		? nullptr
		: FindEntityById(const_cast<std::vector<EntityData>&>(entities), id);
}

EntityData* FindEntityByTag(std::vector<EntityData>& entities, std::string_view tag) {
	const auto it{ std::ranges::find_if(entities, [tag](const EntityData& entity) {
		return entity.tag.View() == tag;
	}) };
	return it != entities.end() ? &*it : nullptr;
}

const DemoRectangle& GetRectangle(const EntityData& entity) {
	return entity.entity.Get<DemoRectangle>();
}

float ApplyDemoEase(float t, Ease ease) {
	t = std::clamp(t, 0.0f, 1.0f);

	switch (ease) {
		case Ease::Linear:	  return t;
		case Ease::InQuad:	  return t * t;
		case Ease::OutQuad:	  return 1.0f - (1.0f - t) * (1.0f - t);
		case Ease::InOutQuad:
			return t < 0.5f ? 2.0f * t * t
						   : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
		case Ease::OutCubic: return 1.0f - std::pow(1.0f - t, 3.0f);
		case Ease::OutBack: {
			constexpr float c1{ 1.70158f };
			constexpr float c3{ c1 + 1.0f };
			return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
		}
	}

	return t;
}

EntityData* ResolveSequenceItemTarget(
	EntityData& owner,
	const RegisteredSequenceItem& item,
	DemoRuntimeContext& context
) {
	if (!context.entities) {
		return &owner;
	}

	if (item.kind != RegisteredSequenceItemKind::MoveTo) {
		return &owner;
	}

	const auto& params{ std::get<MoveToParams>(item.parameters) };
	if (params.target_tag.Empty() || params.target_tag.View() == "Self") {
		return &owner;
	}

	if (auto* target{ FindEntityByTag(*context.entities, params.target_tag.View()) }) {
		return target;
	}

	return &owner;
}

void ExecuteSequenceItem(
	const RegisteredSequenceItem& item,
	EntityData& owner,
	const ScriptSequenceDefinition& sequence,
	DemoRuntimeContext& context,
	bool timed
) {
	EntityData* target{ ResolveSequenceItemTarget(owner, item, context) };
	std::string detail{ GetRegisteredSequenceItemDescriptor(item.kind).label };

	switch (item.kind) {
		case RegisteredSequenceItemKind::SetVisible:
			target->visible = std::get<SetVisibleParams>(item.parameters).visible;
			break;

		case RegisteredSequenceItemKind::MoveTo: {
			if (!timed) {
				const auto& params{ std::get<MoveToParams>(item.parameters) };
				const float x{ params.relative ? target->position[0] + params.destination[0]
											   : params.destination[0] };
				const float y{ params.relative ? target->position[1] + params.destination[1]
											   : params.destination[1] };
				target->position[0] = x;
				target->position[1] = y;
			}
			detail += " -> " + std::string{ target->name.Data() };
			break;
		}

		case RegisteredSequenceItemKind::RotateTo:
			if (!timed) {
				target->rotation = std::get<RotateToParams>(item.parameters).degrees;
			}
			break;

		case RegisteredSequenceItemKind::SpawnEntity: {
			const auto& params{ std::get<SpawnEntityParams>(item.parameters) };
			detail += " [" + std::string{ params.prefab_key.Data() } +
					  ", count=" + std::to_string(std::max(1, params.count)) + "]";
			break;
		}

		case RegisteredSequenceItemKind::AddComponent:
			detail += " [registered components]";
			break;

		case RegisteredSequenceItemKind::RemoveComponent:
			detail += " [registered components]";
			break;

		case RegisteredSequenceItemKind::PlayAudio:
		case RegisteredSequenceItemKind::SetColliderMode:
		case RegisteredSequenceItemKind::ApplyDamage:
			break;
	}

	AddActivity(
		context,
		std::string{ owner.name.Data() } + " / " + sequence.name.Data() +
			(timed ? " completed timed item: " : " executed item: ") + detail
	);
}

void QueueNamedEvent(
	std::string_view event_name,
	const EntityData& owner,
	const ScriptSequenceDefinition& sequence,
	DemoRuntimeContext& context
) {
	if (event_name.empty()) {
		return;
	}

	context.pending_events.push_back(QueuedEvent{
		.kind = TriggerKind::NamedEvent,
		.name = std::string{ event_name },
		.target_entity = 0,
		.source_entity = owner.id,
		.other_entity = 0,
	});

	AddActivity(
		context,
		std::string{ owner.name.Data() } + " / " + sequence.name.Data() +
			" emitted event \"" + std::string{ event_name } + "\""
	);
}

void EmitEvent(
	const EmitEventItem& emit,
	const EntityData& entity,
	const ScriptSequenceDefinition& sequence,
	DemoRuntimeContext& context
) {
	QueueNamedEvent(emit.event_name.View(), entity, sequence, context);
}

void InvokeLifecycleCallbacks(
	LifecycleEventKind event,
	EntityData& entity,
	const ScriptSequenceDefinition& sequence,
	DemoRuntimeContext& context
) {
	for (const auto& callback : sequence.lifecycle_callbacks) {
		if (!callback.enabled || callback.event != event) {
			continue;
		}

		switch (callback.kind) {
			case LifecycleCallbackKind::Item:
				ExecuteSequenceItem(callback.registered_item, entity, sequence, context, false);
				break;

			case LifecycleCallbackKind::EmitEvent:
				QueueNamedEvent(callback.event_name.View(), entity, sequence, context);
				break;
		}
	}
}

bool IsTimedItem(const SequenceItem& item) {
	return item.kind == SequenceItemKind::Wait || item.kind == SequenceItemKind::TimedItem;
}

float GetItemDuration(const SequenceItem& item) {
	switch (item.kind) {
		case SequenceItemKind::Wait:
			return std::max(0.0f, std::get<WaitItem>(item.data).duration_ms);
		case SequenceItemKind::TimedItem:
			return std::max(0.0f, std::get<TimedSequenceItem>(item.data).duration_ms);
		case SequenceItemKind::Item:
		case SequenceItemKind::EmitEvent:
			return 0.0f;
	}
	return 0.0f;
}

void ResetItemRuntime(RuntimeState& runtime) {
	runtime.elapsed_ms = 0.0f;
	runtime.current_repeat = 0;
	runtime.currently_reversed = false;
	runtime.active_target_entity = 0;
	runtime.item_start[0] = 0.0f;
	runtime.item_start[1] = 0.0f;
	runtime.item_end[0] = 0.0f;
	runtime.item_end[1] = 0.0f;
	runtime.start_rotation = 0.0f;
	runtime.end_rotation = 0.0f;
}

void StartScriptSequence(
	EntityData& entity,
	ScriptSequence& binding,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context,
	bool force_restart = true
);

void FinishScriptSequence(
	EntityData& entity,
	ScriptSequence& binding,
	const ScriptSequenceDefinition& sequence,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };
	const bool queued{ runtime.queued };

	runtime.running = false;
	runtime.paused = false;
	runtime.completed = true;
	runtime.queued = false;
	runtime.started_item_index.reset();
	ResetItemRuntime(runtime);
	++runtime.completed_runs;

	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + sequence.name.Data() + " completed"
	);
	InvokeLifecycleCallbacks(LifecycleEventKind::Complete, entity, sequence, context);

	if (sequence.destroy_on_complete) {
		const int completed_runs{ runtime.completed_runs };
		binding.runtime = {};
		binding.runtime.completed_runs = completed_runs;
		return;
	}

	if (queued) {
		StartScriptSequence(entity, binding, registry, context);
	}
}

void BeginTimedSequenceItem(
	EntityData& owner,
	const TimedSequenceItem& timed,
	RuntimeState& runtime,
	DemoRuntimeContext& context
) {
	EntityData* target{ ResolveSequenceItemTarget(owner, timed.registered_item, context) };

	runtime.active_target_entity = target->id;
	runtime.current_repeat = 0;
	runtime.currently_reversed = timed.reversed;
	runtime.elapsed_ms = 0.0f;

	runtime.item_start[0] = target->position[0];
	runtime.item_start[1] = target->position[1];
	runtime.item_end[0] = target->position[0];
	runtime.item_end[1] = target->position[1];
	runtime.start_rotation = target->rotation;
	runtime.end_rotation = target->rotation;

	switch (timed.registered_item.kind) {
		case RegisteredSequenceItemKind::MoveTo: {
			const auto& params{ std::get<MoveToParams>(timed.registered_item.parameters) };
			runtime.item_end[0] = params.relative
				? runtime.item_start[0] + params.destination[0]
				: params.destination[0];
			runtime.item_end[1] = params.relative
				? runtime.item_start[1] + params.destination[1]
				: params.destination[1];
			break;
		}

		case RegisteredSequenceItemKind::RotateTo:
			runtime.end_rotation = std::get<RotateToParams>(timed.registered_item.parameters).degrees;
			break;

		default:
			break;
	}
}

void ApplyTimedSequenceItem(
	const TimedSequenceItem& timed,
	RuntimeState& runtime,
	float linear_progress,
	DemoRuntimeContext& context
) {
	if (!context.entities) {
		return;
	}

	EntityData* target{ FindEntityById(*context.entities, runtime.active_target_entity) };
	if (!target) {
		return;
	}

	const float directed{
		runtime.currently_reversed ? 1.0f - linear_progress : linear_progress
	};
	const float progress{ ApplyDemoEase(directed, timed.ease) };

	switch (timed.registered_item.kind) {
		case RegisteredSequenceItemKind::MoveTo:
			target->position[0] =
				runtime.item_start[0] +
				(runtime.item_end[0] - runtime.item_start[0]) * progress;
			target->position[1] =
				runtime.item_start[1] +
				(runtime.item_end[1] - runtime.item_start[1]) * progress;
			break;

		case RegisteredSequenceItemKind::RotateTo:
			target->rotation =
				runtime.start_rotation +
				(runtime.end_rotation - runtime.start_rotation) * progress;
			break;

		default:
			break;
	}
}

void ProcessImmediateSequenceItems(
	EntityData& entity,
	ScriptSequence& binding,
	const ScriptSequenceDefinition& sequence,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };

	while (runtime.running && runtime.item_index < sequence.sequence.size()) {
		const auto& item{ sequence.sequence[runtime.item_index] };

		if (!item.enabled) {
			++runtime.item_index;
			runtime.started_item_index.reset();
			ResetItemRuntime(runtime);
			continue;
		}

		if (runtime.started_item_index != runtime.item_index) {
			runtime.started_item_index = runtime.item_index;
			InvokeLifecycleCallbacks(LifecycleEventKind::PointStart, entity, sequence, context);

			if (item.kind == SequenceItemKind::TimedItem) {
				BeginTimedSequenceItem(
					entity,
					std::get<TimedSequenceItem>(item.data),
					runtime,
					context
				);
			}
		}

		if (IsTimedItem(item)) {
			if (GetItemDuration(item) <= 0.0f) {
				if (item.kind == SequenceItemKind::TimedItem) {
					const auto& timed{ std::get<TimedSequenceItem>(item.data) };
					ApplyTimedSequenceItem(timed, runtime, 1.0f, context);
					ExecuteSequenceItem(timed.registered_item, entity, sequence, context, true);
				}

				InvokeLifecycleCallbacks(
					LifecycleEventKind::PointComplete, entity, sequence, context
				);
				++runtime.item_index;
				runtime.started_item_index.reset();
				ResetItemRuntime(runtime);
				continue;
			}

			break;
		}

		switch (item.kind) {
			case SequenceItemKind::Item:
				ExecuteSequenceItem(
					std::get<InstantSequenceItem>(item.data).registered_item,
					entity,
					sequence,
					context,
					false
				);
				break;

			case SequenceItemKind::EmitEvent:
				EmitEvent(std::get<EmitEventItem>(item.data), entity, sequence, context);
				break;

			case SequenceItemKind::TimedItem:
			case SequenceItemKind::Wait:
				break;
		}

		InvokeLifecycleCallbacks(LifecycleEventKind::PointComplete, entity, sequence, context);
		++runtime.item_index;
		runtime.started_item_index.reset();
		ResetItemRuntime(runtime);
	}

	if (runtime.running && runtime.item_index >= sequence.sequence.size()) {
		FinishScriptSequence(entity, binding, sequence, registry, context);
	}
}

void StartScriptSequence(
	EntityData& entity,
	ScriptSequence& binding,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context,
	bool force_restart
) {
	if (!binding.enabled) {
		return;
	}

	const auto* sequence{ ResolveScriptSequence(binding, registry) };
	if (!sequence) {
		return;
	}

	auto& runtime{ binding.runtime };

	if (runtime.running && !force_restart) {
		switch (sequence->reentry) {
			case ReentryMode::IgnoreWhileRunning:
				return;

			case ReentryMode::Restart:
				break;

			case ReentryMode::Queue:
				runtime.queued = true;
				return;

			case ReentryMode::Parallel:
				// A single embedded runtime intentionally supports one execution.
				// Parallel can later be implemented as a vector of runtime executions.
				break;
		}
	}

	const bool resetting{
		force_restart &&
		(runtime.running || runtime.paused || runtime.completed ||
		 runtime.started_item_index.has_value())
	};

	if (resetting) {
		InvokeLifecycleCallbacks(LifecycleEventKind::Reset, entity, *sequence, context);
	}

	runtime.running = !sequence->sequence.empty();
	runtime.paused = false;
	runtime.completed = false;
	runtime.queued = false;
	runtime.item_index = 0;
	runtime.started_item_index.reset();
	ResetItemRuntime(runtime);

	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + sequence->name.Data() + " started"
	);
	InvokeLifecycleCallbacks(LifecycleEventKind::Start, entity, *sequence, context);

	if (runtime.running) {
		ProcessImmediateSequenceItems(entity, binding, *sequence, registry, context);
	} else {
		FinishScriptSequence(entity, binding, *sequence, registry, context);
	}
}

void SetScriptSequencePaused(
	EntityData& entity,
	ScriptSequence& binding,
	const ScriptSequenceDefinition& sequence,
	DemoRuntimeContext& context,
	bool paused
) {
	if (!binding.runtime.running || binding.runtime.paused == paused) {
		return;
	}

	binding.runtime.paused = paused;
	InvokeLifecycleCallbacks(
		paused ? LifecycleEventKind::Pause : LifecycleEventKind::Resume,
		entity,
		sequence,
		context
	);
	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + sequence.name.Data() +
			(paused ? " paused" : " resumed")
	);
}

void StopScriptSequence(
	EntityData& entity,
	ScriptSequence& binding,
	const ScriptSequenceDefinition& sequence,
	DemoRuntimeContext& context
) {
	if (!binding.runtime.running && !binding.runtime.paused) {
		binding.runtime = {};
		return;
	}

	InvokeLifecycleCallbacks(LifecycleEventKind::Stop, entity, sequence, context);
	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + sequence.name.Data() + " stopped"
	);
	binding.runtime = {};
}

void UpdateScriptSequence(
	EntityData& entity,
	ScriptSequence& binding,
	float delta_seconds,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };
	if (!runtime.running || runtime.paused) {
		return;
	}

	const auto* sequence{ ResolveScriptSequence(binding, registry) };
	if (!sequence || runtime.item_index >= sequence->sequence.size()) {
		if (sequence) {
			FinishScriptSequence(entity, binding, *sequence, registry, context);
		}
		return;
	}

	const auto& item{ sequence->sequence[runtime.item_index] };
	if (!item.enabled || !IsTimedItem(item)) {
		ProcessImmediateSequenceItems(entity, binding, *sequence, registry, context);
		return;
	}

	if (runtime.started_item_index != runtime.item_index) {
		runtime.started_item_index = runtime.item_index;
		InvokeLifecycleCallbacks(LifecycleEventKind::PointStart, entity, *sequence, context);

		if (item.kind == SequenceItemKind::TimedItem) {
			BeginTimedSequenceItem(
				entity,
				std::get<TimedSequenceItem>(item.data),
				runtime,
				context
			);
		}
	}

	const float duration{ GetItemDuration(item) };
	runtime.elapsed_ms += delta_seconds * 1000.0f;
	const float linear_progress{
		duration <= 0.0f ? 1.0f : std::clamp(runtime.elapsed_ms / duration, 0.0f, 1.0f)
	};

	if (item.kind == SequenceItemKind::TimedItem) {
		ApplyTimedSequenceItem(
			std::get<TimedSequenceItem>(item.data),
			runtime,
			linear_progress,
			context
		);
	}

	if (linear_progress < 1.0f) {
		return;
	}

	if (item.kind == SequenceItemKind::TimedItem) {
		const auto& timed{ std::get<TimedSequenceItem>(item.data) };
		const bool repeat{
			timed.infinite_repeats || runtime.current_repeat < timed.additional_repeats
		};

		if (repeat) {
			++runtime.current_repeat;
			runtime.elapsed_ms = 0.0f;

			if (timed.yoyo) {
				runtime.currently_reversed = !runtime.currently_reversed;
				InvokeLifecycleCallbacks(LifecycleEventKind::Yoyo, entity, *sequence, context);
			} else {
				InvokeLifecycleCallbacks(LifecycleEventKind::Repeat, entity, *sequence, context);
			}

			return;
		}

		ExecuteSequenceItem(timed.registered_item, entity, *sequence, context, true);
	}

	InvokeLifecycleCallbacks(LifecycleEventKind::PointComplete, entity, *sequence, context);
	++runtime.item_index;
	runtime.started_item_index.reset();
	ResetItemRuntime(runtime);
	ProcessImmediateSequenceItems(entity, binding, *sequence, registry, context);
}

bool MatchesEventTrigger(
	const TriggerDefinition& trigger,
	const QueuedEvent& event,
	const EntityData& owner,
	const std::vector<EntityData>& entities
) {
	if (!trigger.enabled || trigger.kind != event.kind) {
		return false;
	}

	switch (trigger.kind) {
		case TriggerKind::NamedEvent:
			return trigger.event_name.View() == event.name;

		case TriggerKind::KeyPressed:
		case TriggerKind::KeyReleased:
		case TriggerKind::KeyHeld:
			return trigger.key.View() == event.name;

		case TriggerKind::MousePressed:
		case TriggerKind::MouseReleased:
		case TriggerKind::MouseHeld:
			return trigger.mouse_button.View() == event.name;

		case TriggerKind::OverlapStart:
		case TriggerKind::OverlapStop:
		case TriggerKind::CollisionStart:
		case TriggerKind::CollisionStop: {
			const EntityData* other{ FindEntityById(entities, event.other_entity) };
			return other && (trigger.tag_filter.Empty() ||
							 trigger.tag_filter.View() == other->tag.View());
		}

		case TriggerKind::OnCreate:
			return event.target_entity == owner.id;
	}

	return false;
}

void DispatchEvents(
	std::vector<EntityData>& entities,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context
) {
	while (!context.pending_events.empty()) {
		QueuedEvent event{ std::move(context.pending_events.front()) };
		context.pending_events.pop_front();

		for (auto& entity : entities) {
			if (event.target_entity != 0 && event.target_entity != entity.id) {
				continue;
			}

			auto* scripts{ TryGetScripts(entity) };
			if (!scripts) {
				continue;
			}

			for (auto& binding : scripts->sequences) {
				const auto* sequence{ ResolveScriptSequence(binding, registry) };
				if (!sequence) {
					continue;
				}

				const bool stop_matches{
					std::ranges::any_of(sequence->stop_triggers, [&](const TriggerDefinition& trigger) {
						return MatchesEventTrigger(trigger, event, entity, entities);
					})
				};

				if (stop_matches) {
					AddActivity(
						context,
						std::string{ entity.name.Data() } + " / " + sequence->name.Data() +
							" matched stop event"
					);
					StopScriptSequence(entity, binding, *sequence, context);
					continue;
				}

				const bool start_matches{
					std::ranges::any_of(sequence->triggers, [&](const TriggerDefinition& trigger) {
						return MatchesEventTrigger(trigger, event, entity, entities);
					})
				};

				if (start_matches) {
					AddActivity(
						context,
						std::string{ entity.name.Data() } + " / " + sequence->name.Data() +
							" matched start event"
					);
					StartScriptSequence(entity, binding, registry, context, false);
				}
			}
		}
	}
}

bool RectanglesOverlap(const EntityData& a, const EntityData& b) {
	const auto& a_rect{ GetRectangle(a) };
	const auto& b_rect{ GetRectangle(b) };

	const float a_half_x{ a_rect.size[0] * 0.5f * std::abs(a.scale[0]) };
	const float a_half_y{ a_rect.size[1] * 0.5f * std::abs(a.scale[1]) };
	const float b_half_x{ b_rect.size[0] * 0.5f * std::abs(b.scale[0]) };
	const float b_half_y{ b_rect.size[1] * 0.5f * std::abs(b.scale[1]) };

	return std::abs(a.position[0] - b.position[0]) <= a_half_x + b_half_x &&
		   std::abs(a.position[1] - b.position[1]) <= a_half_y + b_half_y;
}

void UpdatePlayerInput(
	GLFWwindow* window,
	std::vector<EntityData>& entities,
	float delta_seconds
) {
	EntityData* player{ nullptr };

	for (auto& entity : entities) {
		if (entity.entity.Has<DemoPlayer>()) {
			player = &entity;
			break;
		}
	}

	if (!player || ImGui::GetIO().WantTextInput) {
		return;
	}

	float x{ 0.0f };
	float y{ 0.0f };

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		x -= 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		x += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		y -= 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		y += 1.0f;
	}

	const float magnitude{ std::sqrt(x * x + y * y) };
	if (magnitude > 0.0f) {
		x /= magnitude;
		y /= magnitude;
	}

	const float speed{ player->entity.Get<DemoPlayer>().speed };
	player->position[0] += x * speed * delta_seconds;
	player->position[1] += y * speed * delta_seconds;

	player->position[0] = std::clamp(player->position[0], -430.0f, 430.0f);
	player->position[1] = std::clamp(player->position[1], -250.0f, 250.0f);
}

void UpdateOverlapEvents(
	std::vector<EntityData>& entities,
	DemoRuntimeContext& context
) {
	EntityData* player{ nullptr };
	EntityData* sensor{ nullptr };

	for (auto& entity : entities) {
		if (entity.entity.Has<DemoPlayer>()) {
			player = &entity;
		}
		if (entity.entity.Has<DemoDoorSensor>()) {
			sensor = &entity;
		}
	}

	if (!player || !sensor) {
		return;
	}

	const bool overlapping{ RectanglesOverlap(*player, *sensor) };
	if (overlapping == context.player_overlapping_door) {
		return;
	}

	context.player_overlapping_door = overlapping;
	context.pending_events.push_back(QueuedEvent{
		.kind = overlapping ? TriggerKind::OverlapStart : TriggerKind::OverlapStop,
		.name = {},
		.target_entity = sensor->id,
		.source_entity = sensor->id,
		.other_entity = player->id,
	});

	AddActivity(
		context,
		std::string{ overlapping ? "OverlapStart" : "OverlapStop" } +
			"(Door Sensor, Player)"
	);
}

void UpdateActivity(DemoRuntimeContext&, float) {}

float GetRuntimeProgress(const ScriptSequence& binding, const ScriptSequenceDefinition& sequence) {
	const auto& runtime{ binding.runtime };
	if (runtime.completed) {
		return 1.0f;
	}
	if (!runtime.running || runtime.item_index >= sequence.sequence.size()) {
		return 0.0f;
	}
	const float duration{ GetItemDuration(sequence.sequence[runtime.item_index]) };
	return duration > 0.0f ? std::clamp(runtime.elapsed_ms / duration, 0.0f, 1.0f) : 0.0f;
}

std::string TriggerSummary(const TriggerDefinition& trigger) {
	switch (trigger.kind) {
		case TriggerKind::OnCreate: {
			char buffer[64]{};
			char duration[32]{};
			FormatDurationMilliseconds(trigger.duration_ms, duration, sizeof(duration));
			std::snprintf(buffer, sizeof(buffer), "On Create — Delay: %s", duration);
			return buffer;
		}

		case TriggerKind::NamedEvent:	   return std::string{ "Event: " } + trigger.event_name.Data();

		case TriggerKind::KeyPressed:  return std::string{ "Key Pressed: " } + trigger.key.Data();

		case TriggerKind::KeyReleased: return std::string{ "Key Released: " } + trigger.key.Data();

		case TriggerKind::KeyHeld:	   return std::string{ "Key Held: " } + trigger.key.Data();

		case TriggerKind::MousePressed:
			return std::string{ "Mouse Pressed: " } + trigger.mouse_button.Data();

		case TriggerKind::MouseReleased:
			return std::string{ "Mouse Released: " } + trigger.mouse_button.Data();

		case TriggerKind::MouseHeld:
			return std::string{ "Mouse Held: " } + trigger.mouse_button.Data();

		case TriggerKind::OverlapStart:
			return std::string{ "Overlap Start: " } + trigger.tag_filter.Data();

		case TriggerKind::OverlapStop:
			return std::string{ "Overlap Stop: " } + trigger.tag_filter.Data();

		case TriggerKind::CollisionStart:
			return std::string{ "Collision Start: " } + trigger.tag_filter.Data();

		case TriggerKind::CollisionStop:
			return std::string{ "Collision Stop: " } + trigger.tag_filter.Data();
	}

	return {};
}

std::string SequenceItemSummary(const SequenceItem& item) {
	char buffer[160]{};
	switch (item.kind) {
		case SequenceItemKind::Item:
			return GetRegisteredSequenceItemDescriptor(std::get<InstantSequenceItem>(item.data).registered_item.kind).label;
		case SequenceItemKind::TimedItem: {
			const auto& timed{ std::get<TimedSequenceItem>(item.data) };
			std::snprintf(buffer, sizeof(buffer), "%s  %.0fms", GetRegisteredSequenceItemDescriptor(timed.registered_item.kind).label, timed.duration_ms);
			return buffer;
		}
		case SequenceItemKind::Wait:
			std::snprintf(buffer, sizeof(buffer), "%.0fms", std::get<WaitItem>(item.data).duration_ms);
			return buffer;
		case SequenceItemKind::EmitEvent:
			return std::get<EmitEventItem>(item.data).event_name.Data();
	}
	return {};
}


bool DrawComponentKindCombo(
	const char* label, ComponentDefinition& component, float width = -FLT_MIN
) {
	ImGui::SetNextItemWidth(width);

	const auto& current{ GetComponentDescriptor(component.kind) };
	bool changed{ false };

	if (ImGui::BeginCombo(label, current.label)) {
		constexpr std::array groups{ "Core", "Graphics", "Physics", "Gameplay" };

		for (const char* group : groups) {
			if (!ImGui::BeginMenu(group)) {
				continue;
			}

			for (const auto& descriptor : kComponentRegistry) {
				if (std::strcmp(descriptor.group, group) != 0) {
					continue;
				}

				const bool selected{ component.kind == descriptor.kind };

				if (ImGui::MenuItem(descriptor.label, nullptr, selected)) {
					const Id id{ component.id };
					component = MakeComponent(descriptor.kind);
					component.id = id;
					changed = true;
				}

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s\n%s", descriptor.description, descriptor.key);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndCombo();
	}

	return changed;
}

bool ContainsComponentKind(const std::vector<ComponentKind>& components, ComponentKind kind) {
	return std::ranges::find(components, kind) != components.end();
}

void RemoveComponentKind(std::vector<ComponentKind>& components, ComponentKind kind) {
	std::erase(components, kind);
}

std::string BuildSelectionTooltip(
	std::string_view heading, const std::vector<std::string_view>& selected,
	std::string_view empty_item
) {
	std::string tooltip{ heading };

	if (selected.empty()) {
		tooltip += "\n";
		tooltip += empty_item;
		return tooltip;
	}

	for (const auto item : selected) {
		tooltip += "\n- ";
		tooltip += item;
	}

	return tooltip;
}

std::string BuildCommaSeparatedTooltip(
	std::string_view explanation, std::string_view comma_separated_values
) {
	std::string tooltip{ explanation };
	std::size_t start{ 0 };

	while (start <= comma_separated_values.size()) {
		const std::size_t comma{ comma_separated_values.find(',', start) };
		const std::size_t end{ comma == std::string_view::npos ? comma_separated_values.size()
															   : comma };
		std::string_view item{ comma_separated_values.substr(start, end - start) };

		while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
			item.remove_prefix(1);
		}
		while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
			item.remove_suffix(1);
		}

		if (!item.empty()) {
			tooltip += "\n- ";
			tooltip += item;
		}

		if (comma == std::string_view::npos) {
			break;
		}
		start = comma + 1;
	}

	return tooltip;
}

std::vector<std::string_view> GetComponentSelectionLabels(
	const std::vector<ComponentKind>& selected
) {
	std::vector<std::string_view> labels;
	labels.reserve(selected.size());
	for (const auto kind : selected) {
		labels.emplace_back(GetComponentDescriptor(kind).label);
	}
	return labels;
}

void DrawComponentSelectionTooltip(
	const std::vector<ComponentKind>& selected, std::string_view heading = "Selected components:"
) {
	if (!ImGui::IsItemHovered()) {
		return;
	}
	const auto labels{ GetComponentSelectionLabels(selected) };
	const std::string tooltip{ BuildSelectionTooltip(heading, labels) };
	ImGui::SetTooltip("%s", tooltip.c_str());
}

bool DrawComponentMultiSelectCombo(
	const char* label, std::vector<ComponentKind>& selected, const char* empty_text,
	float width = -FLT_MIN, std::vector<ComponentKind>* mutually_exclusive = nullptr,
	std::string_view tooltip_heading = "Selected components:"
) {
	const std::string preview{ ComponentSelectionPreview(selected, empty_text) };
	ImGui::SetNextItemWidth(width);

	bool changed{ false };
	const bool open{ ImGui::BeginCombo(label, preview.c_str()) };
	DrawComponentSelectionTooltip(selected, tooltip_heading);

	if (open) {
		constexpr std::array groups{ "Core", "Graphics", "Physics", "Gameplay" };

		for (const char* group : groups) {
			if (!ImGui::BeginMenu(group)) {
				continue;
			}

			for (const auto& descriptor : kComponentRegistry) {
				if (std::strcmp(descriptor.group, group) != 0) {
					continue;
				}

				bool checked{ ContainsComponentKind(selected, descriptor.kind) };

				if (ImGui::Checkbox(descriptor.label, &checked)) {
					if (checked) {
						selected.push_back(descriptor.kind);
						if (mutually_exclusive) {
							RemoveComponentKind(*mutually_exclusive, descriptor.kind);
						}
					} else {
						RemoveComponentKind(selected, descriptor.kind);
					}
					changed = true;
				}

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s\n%s", descriptor.description, descriptor.key);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndCombo();
	}

	return changed;
}

bool HasAddedComponent(const AddComponentParams& params, ComponentKind kind) {
	return std::ranges::any_of(params.components, [kind](const auto& component) {
		return component.kind == kind;
	});
}

bool CanAddComponent(const AddComponentParams& params) {
	return params.components.size() < kComponentRegistry.size();
}

void DrawAddComponentButton(RegisteredSequenceItem& action) {
	auto* params{ std::get_if<AddComponentParams>(&action.parameters) };
	if (action.kind != RegisteredSequenceItemKind::AddComponent || !params) {
		return;
	}

	const float button_size{ ImGui::GetFrameHeight() };

	ImGui::BeginDisabled(!CanAddComponent(*params));
	if (ImGui::Button("+", ImVec2{ button_size, button_size })) {
		ImGui::OpenPopup("AddComponentPopup");
	}
	ImGui::EndDisabled();
	DrawItemTooltip(
		CanAddComponent(*params) ? "Add a registered component."
								 : "Every registered component is already selected."
	);

	if (ImGui::BeginPopup("AddComponentPopup")) {
		constexpr std::array groups{ "Core", "Graphics", "Physics", "Gameplay" };

		for (const char* group : groups) {
			if (!ImGui::BeginMenu(group)) {
				continue;
			}

			for (const auto& descriptor : kComponentRegistry) {
				if (std::strcmp(descriptor.group, group) != 0) {
					continue;
				}

				const bool already_added{ HasAddedComponent(*params, descriptor.kind) };
				ImGui::BeginDisabled(already_added);
				if (ImGui::MenuItem(descriptor.label)) {
					params->components.push_back(MakeComponent(descriptor.kind));
				}
				ImGui::EndDisabled();

				if (ImGui::IsItemHovered(already_added ? ImGuiHoveredFlags_AllowWhenDisabled : 0)) {
					ImGui::SetTooltip(
						already_added ? "Already selected.\n%s\n%s" : "%s\n%s",
						descriptor.description, descriptor.key
					);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
}

std::vector<std::string_view> GetSpawnOptionLabels(const SpawnEntityParams& spawn) {
	std::vector<std::string_view> labels;
	if (spawn.parent_to_owner) {
		labels.emplace_back("Parent to Owner");
	}
	if (spawn.inherit_owner_rotation) {
		labels.emplace_back("Parent Rotation");
	}
	if (spawn.inherit_owner_scale) {
		labels.emplace_back("Parent Scale");
	}
	if (spawn.random_rotation) {
		labels.emplace_back("Random Rotation");
	}
	return labels;
}

std::string SpawnOptionsPreview(const SpawnEntityParams& spawn) {
	const auto labels{ GetSpawnOptionLabels(spawn) };
	if (labels.empty()) {
		return "Options";
	}

	std::string preview;
	for (const auto label : labels) {
		if (!preview.empty()) {
			preview += ", ";
		}
		preview += label;
	}
	return preview;
}

void DrawSpawnOptionsCombo(SpawnEntityParams& spawn) {
	if (spawn.inherit_owner_rotation && spawn.random_rotation) {
		spawn.random_rotation = false;
	}

	const std::string preview{ SpawnOptionsPreview(spawn) };
	ImGui::SetNextItemWidth(-FLT_MIN);
	const bool open{ ImGui::BeginCombo("##SpawnOptions", preview.c_str()) };

	if (ImGui::IsItemHovered()) {
		const auto labels{ GetSpawnOptionLabels(spawn) };
		const std::string tooltip{ BuildSelectionTooltip("Selected options:", labels) };
		ImGui::SetTooltip("%s", tooltip.c_str());
	}

	if (open) {
		ImGui::Checkbox("Parent to Owner", &spawn.parent_to_owner);
		DrawItemTooltip("Make the sequence owner the spawned entity's parent.");

		ImGui::BeginDisabled(spawn.random_rotation);
		bool parent_rotation{ spawn.inherit_owner_rotation };
		if (ImGui::Checkbox("Parent Rotation", &parent_rotation)) {
			spawn.inherit_owner_rotation = parent_rotation;
			if (parent_rotation) {
				spawn.random_rotation = false;
			}
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(spawn.random_rotation ? ImGuiHoveredFlags_AllowWhenDisabled : 0)) {
			ImGui::SetTooltip(
				"%s", spawn.random_rotation
						  ? "Disabled while Random Rotation is selected."
						  : "Apply the sequence owner's rotation to each spawned entity."
			);
		}

		ImGui::Checkbox("Parent Scale", &spawn.inherit_owner_scale);
		DrawItemTooltip("Apply the sequence owner's scale to each spawned entity.");

		ImGui::BeginDisabled(spawn.inherit_owner_rotation);
		bool random_rotation{ spawn.random_rotation };
		if (ImGui::Checkbox("Random Rotation", &random_rotation)) {
			spawn.random_rotation = random_rotation;
			if (random_rotation) {
				spawn.inherit_owner_rotation = false;
			}
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(
				spawn.inherit_owner_rotation ? ImGuiHoveredFlags_AllowWhenDisabled : 0
			)) {
			ImGui::SetTooltip(
				"%s", spawn.inherit_owner_rotation
						  ? "Disabled while Parent Rotation is selected."
						  : "Give each spawned entity a uniformly random rotation."
			);
		}

		ImGui::EndCombo();
	}
}

bool DrawPrefabKeyPicker(
	const char* label, TextBuffer<96>& prefab_key, const PrefabRegistry& prefabs,
	float width = -FLT_MIN
) {
	ImGui::SetNextItemWidth(width);

	const char* preview{ prefab_key.Empty() ? "Select prefab key" : prefab_key.Data() };
	bool changed{ false };
	const bool open{ ImGui::BeginCombo(label, preview) };

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Selected prefab:\n%s", prefab_key.Empty() ? "(none)" : prefab_key.Data());
	}

	if (open) {
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::InputTextWithHint(
				"##CustomPrefabKey", "Type prefab key", prefab_key.Data(), prefab_key.Size()
			)) {
			changed = true;
		}
		DrawItemTooltip("A custom key may reference a prefab asset that is not loaded in this demo.");

		if (!prefabs.definitions.empty()) {
			ImGui::Separator();
		}

		for (const auto& prefab : prefabs.definitions) {
			const bool selected{ prefab.key.View() == prefab_key.View() };

			if (ImGui::MenuItem(prefab.key.Data(), nullptr, selected)) {
				prefab_key.Assign(prefab.key.View());
				changed = true;
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s\nTag: %s", prefab.name.Data(),
					prefab.tag.Empty() ? "(none)" : prefab.tag.Data());
			}
		}

		ImGui::EndCombo();
	}

	return changed;
}

void DrawComponentMembers(ComponentDefinition& component, float left_screen_x) {
	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	const float available_width{ std::max(1.0f, right_screen_x - left_screen_x) };

	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	ImGui::PushID(static_cast<int>(component.id));

	switch (component.kind) {
		case ComponentKind::Transform: {
			auto& data{ std::get<TransformComponentData>(component.data) };

			if (ImGui::BeginTable(
					"TransformMembers", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Rotation", ImGuiTableColumnFlags_WidthFixed, 100.0f);
				ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat2(
					"##Position", data.position, 1.0f, -100000.0f, 100000.0f, "P %.0f"
				);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##Rotation", &data.rotation, 1.0f, -3600.0f, 3600.0f, "%.1f deg"
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat2(
					"##Scale", data.scale, 0.01f, -1000.0f, 1000.0f, "S %.2f"
				);
				ImGui::EndTable();
			}
			break;
		}

		case ComponentKind::Visible: {
			auto& data{ std::get<VisibleComponentData>(component.data) };
			ImGui::Checkbox("Visible", &data.visible);
			break;
		}

		case ComponentKind::Sprite: {
			auto& data{ std::get<SpriteComponentData>(component.data) };

			if (ImGui::BeginTable(
					"SpriteMembers", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 142.0f);
				ImGui::TableSetupColumn("Flip", ImGuiTableColumnFlags_WidthFixed, 58.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint(
					"##Texture", "Texture key", data.texture.Data(), data.texture.Size()
				);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat2(
					"##Size", data.size, 1.0f, 0.0f, 100000.0f, "%.0f"
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::Checkbox("Flip X", &data.flip_x);
				ImGui::EndTable();
			}
			break;
		}

		case ComponentKind::Collider: {
			auto& data{ std::get<ColliderComponentData>(component.data) };

			if (ImGui::BeginTable(
					"ColliderMembers", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Mask", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo(
						"##Mode", kColliderModeNames[static_cast<std::size_t>(data.mode)]
					)) {
					for (int i{ 0 }; i < static_cast<int>(kColliderModeNames.size()); ++i) {
						const bool selected{ data.mode == i };
						if (ImGui::Selectable(
								kColliderModeNames[static_cast<std::size_t>(i)], selected
							)) {
							data.mode = i;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat("##Radius", &data.radius, 0.5f, 0.0f, 100000.0f, "R %.1f");

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputInt("##Mask", &data.mask);
				ImGui::EndTable();
			}
			break;
		}

		case ComponentKind::Health: {
			auto& data{ std::get<HealthComponentData>(component.data) };

			if (ImGui::BeginTable(
					"HealthMembers", 2, ImGuiTableFlags_SizingStretchSame,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##Maximum", &data.maximum, 1.0f, 0.0f, 100000.0f, "Maximum %.0f"
				);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##Current", &data.current, 1.0f, 0.0f, 100000.0f, "Current %.0f"
				);
				ImGui::EndTable();
			}
			break;
		}

		case ComponentKind::Zombie:
			ImGui::TextDisabled("Tag component: no reflected members.");
			break;

		case ComponentKind::Damage: {
			auto& data{ std::get<DamageComponentData>(component.data) };

			if (ImGui::BeginTable(
					"DamageMembers", 2, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 115.0f);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat("##Damage", &data.amount, 0.5f, 0.0f, 100000.0f, "%.1f");

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint(
					"##DamageType", "Damage type", data.damage_type.Data(),
					data.damage_type.Size()
				);
				ImGui::EndTable();
			}
			break;
		}

		case ComponentKind::Lifetime: {
			auto& data{ std::get<LifetimeComponentData>(component.data) };
			DrawDurationInput(
				"##Lifetime", data.duration_ms, available_width,
				"Runtime lifetime before the spawned entity is destroyed."
			);
			break;
		}
	}

	ImGui::PopID();
}

void DrawRegisteredSequenceItemParametersCompact(
	RegisteredSequenceItem& action, float left_screen_x, const PrefabRegistry& prefabs
) {
	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	const float available_width{ std::max(1.0f, right_screen_x - left_screen_x) };

	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });

	switch (action.kind) {
		case RegisteredSequenceItemKind::SetVisible: {
			auto& p{ std::get<SetVisibleParams>(action.parameters) };
			ImGui::Checkbox("Visible", &p.visible);
			break;
		}

		case RegisteredSequenceItemKind::MoveTo: {
			auto& p{ std::get<MoveToParams>(action.parameters) };

			ImGui::SetNextItemWidth(available_width);
			ImGui::InputTextWithHint(
				"##MoveTargetTag", "Target tag, or Self", p.target_tag.Data(), p.target_tag.Size()
			);
			DrawItemTooltip(
				"Entity targeted by this registered sequence item. Use Self for the owner."
			);

			if (ImGui::BeginTable(
					"MoveToParams", 4, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 53.0f);
				ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Relative", ImGuiTableColumnFlags_WidthFixed, 88.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("Position");

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##DestinationX", &p.destination[0], 1.0f, -100000.0f, 100000.0f, "X: %.0f"
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##DestinationY", &p.destination[1], 1.0f, -100000.0f, 100000.0f, "Y: %.0f"
				);

				ImGui::TableSetColumnIndex(3);
				ImGui::Checkbox("Relative", &p.relative);
				ImGui::EndTable();
			}
			break;
		}

		case RegisteredSequenceItemKind::RotateTo: {
			auto& p{ std::get<RotateToParams>(action.parameters) };

			if (ImGui::BeginTable(
					"RotateToParams", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
				ImGui::TableSetupColumn("Angle", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Shortest", ImGuiTableColumnFlags_WidthFixed, 82.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("Angle");

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat("##Degrees", &p.degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg");

				ImGui::TableSetColumnIndex(2);
				ImGui::Checkbox("Shortest", &p.shortest_path);
				DrawItemTooltip("Uses the shortest rotational path to the target angle.");
				ImGui::EndTable();
			}
			break;
		}

		case RegisteredSequenceItemKind::PlayAudio: {
			auto& p{ std::get<PlayAudioParams>(action.parameters) };

			if (ImGui::BeginTable(
					"AudioParams", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn(
					"Loops", ImGuiTableColumnFlags_WidthFixed, GetCountControlWidth("Loops")
				);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Audio", p.asset.Data(), p.asset.Size());

				ImGui::TableSetColumnIndex(1);
				DrawVolumeControl(p.volume);

				ImGui::TableSetColumnIndex(2);
				DrawCountControl("Loops", p.loops, 0);

				ImGui::EndTable();
			}
			break;
		}

		case RegisteredSequenceItemKind::SetColliderMode: {
			auto& p{ std::get<SetColliderModeParams>(action.parameters) };
			ImGui::SetNextItemWidth(available_width);

			if (ImGui::BeginCombo(
					"##ColliderMode", kColliderModeNames[static_cast<std::size_t>(p.mode)]
				)) {
				for (int i{ 0 }; i < static_cast<int>(kColliderModeNames.size()); ++i) {
					const bool selected{ p.mode == i };

					if (ImGui::Selectable(
							kColliderModeNames[static_cast<std::size_t>(i)], selected
						)) {
						p.mode = i;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
			break;
		}

		case RegisteredSequenceItemKind::ApplyDamage: {
			auto& p{ std::get<ApplyDamageParams>(action.parameters) };

			if (ImGui::BeginTable(
					"DamageParams", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 92.0f);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Critical", ImGuiTableColumnFlags_WidthFixed, 62.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat("##Amount", &p.amount, 0.25f, 0.0f, 100000.0f, "%.2f");

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##DamageType", p.damage_type.Data(), p.damage_type.Size());

				ImGui::TableSetColumnIndex(2);
				ImGui::Checkbox("Critical", &p.critical);
				ImGui::EndTable();
			}
			break;
		}

		case RegisteredSequenceItemKind::SpawnEntity: {
			auto& spawn{ std::get<SpawnEntityParams>(action.parameters) };
			spawn.count				= std::clamp(spawn.count, 1, 100);
			spawn.rectangle_size[0] = std::max(0.0f, spawn.rectangle_size[0]);
			spawn.rectangle_size[1] = std::max(0.0f, spawn.rectangle_size[1]);
			spawn.radius			= std::max(0.0f, spawn.radius);

			const float count_control_width{ GetCountControlWidth("Count") };

			ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
			if (ImGui::BeginTable(
					"SpawnEntityPrimaryRow", 2, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Prefab", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(
					"Count", ImGuiTableColumnFlags_WidthFixed, count_control_width
				);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				DrawPrefabKeyPicker("##PrefabKey", spawn.prefab_key, prefabs);

				ImGui::TableSetColumnIndex(1);
				DrawCountControl(
					"Count", spawn.count, 1, 100, false,
					"Number of prefab instances created by this action."
				);
				ImGui::EndTable();
			}

			const SpawnArea displayed_area{ spawn.area };
			int placement_columns{ 4 };
			if (displayed_area == SpawnArea::Rectangle) {
				placement_columns += 2;
			} else if (displayed_area == SpawnArea::Circle) {
				++placement_columns;
			}

			ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
			if (ImGui::BeginTable(
					"SpawnEntityPlacementRow", placement_columns, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch, 0.8f);
				ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch, 0.8f);
				ImGui::TableSetupColumn("Shape", ImGuiTableColumnFlags_WidthFixed, 90.0f);

				if (displayed_area == SpawnArea::Rectangle) {
					ImGui::TableSetupColumn("Width", ImGuiTableColumnFlags_WidthStretch, 0.8f);
					ImGui::TableSetupColumn("Height", ImGuiTableColumnFlags_WidthStretch, 0.8f);
				} else if (displayed_area == SpawnArea::Circle) {
					ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				}

				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				DrawSpawnOriginCombo("##SpawnOrigin", spawn.origin);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##SpawnX", &spawn.center[0], 1.0f, -100000.0f, 100000.0f, "X: %.0f"
				);
				DrawItemTooltip(
					spawn.origin == SpawnOrigin::OwnerEntity
						? "Horizontal offset from the sequence entity."
						: "World-space X position."
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##SpawnY", &spawn.center[1], 1.0f, -100000.0f, 100000.0f, "Y: %.0f"
				);
				DrawItemTooltip(
					spawn.origin == SpawnOrigin::OwnerEntity
						? "Vertical offset from the sequence entity."
						: "World-space Y position."
				);

				ImGui::TableSetColumnIndex(3);
				DrawEnumCombo("##SpawnArea", spawn.area, kSpawnAreaNames);
				DrawItemTooltip(
					"Point uses the position exactly. Rectangle and Circle choose a uniformly "
					"random position inside the selected area."
				);

				if (displayed_area == SpawnArea::Rectangle) {
					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::DragFloat(
							"##SpawnRectangleWidth", &spawn.rectangle_size[0], 1.0f, 0.0f,
							100000.0f, "W: %.0f"
						)) {
						spawn.rectangle_size[0] = std::max(0.0f, spawn.rectangle_size[0]);
					}
					DrawItemTooltip("Full width of the uniformly sampled spawn rectangle.");

					ImGui::TableSetColumnIndex(5);
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::DragFloat(
							"##SpawnRectangleHeight", &spawn.rectangle_size[1], 1.0f, 0.0f,
							100000.0f, "H: %.0f"
						)) {
						spawn.rectangle_size[1] = std::max(0.0f, spawn.rectangle_size[1]);
					}
					DrawItemTooltip("Full height of the uniformly sampled spawn rectangle.");
				} else if (displayed_area == SpawnArea::Circle) {
					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::DragFloat(
							"##SpawnRadius", &spawn.radius, 1.0f, 0.0f, 100000.0f, "R: %.0f"
						)) {
						spawn.radius = std::max(0.0f, spawn.radius);
					}
					DrawItemTooltip(
						"Radius of the random spawn circle. Positions are sampled uniformly by "
						"area."
					);
				}

				ImGui::EndTable();
			}

			ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
			DrawSpawnOptionsCombo(spawn);
			break;
		}

		case RegisteredSequenceItemKind::AddComponent: {
			auto& p{ std::get<AddComponentParams>(action.parameters) };
			int remove_component{ -1 };

			for (int i{ 0 }; i < static_cast<int>(p.components.size()); ++i) {
				auto& component{ p.components[static_cast<std::size_t>(i)] };
				ImGui::PushID(static_cast<int>(component.id));
				ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });

				if (ImGui::BeginTable(
						"AddComponentTitle", 2, ImGuiTableFlags_SizingStretchProp,
						ImVec2{ available_width, 0.0f }
					)) {
					ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn(
						"Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight()
					);
					ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(GetComponentDescriptor(component.kind).label);

					ImGui::TableSetColumnIndex(1);
					if (ImGui::Button(
							"x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }
						)) {
						remove_component = i;
					}
					ImGui::EndTable();
				}

				DrawComponentMembers(component, left_screen_x);
				ImGui::PopID();
			}

			if (remove_component >= 0) {
				p.components.erase(p.components.begin() + remove_component);
			}
			break;
		}

		case RegisteredSequenceItemKind::RemoveComponent: {
			auto& p{ std::get<RemoveComponentParams>(action.parameters) };
			ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
			DrawComponentMultiSelectCombo(
				"##RemoveComponents", p.components, "Select components to remove", available_width
			);
			break;
		}
	}
}

void DrawRegisteredSequenceItemParameters(RegisteredSequenceItem& action, const PrefabRegistry& prefabs) {
	DrawRegisteredSequenceItemParametersCompact(action, ImGui::GetCursorScreenPos().x, prefabs);
}

bool DrawRegisteredSequenceItemPicker(
	const char* label,
	RegisteredSequenceItem& action,
	bool timed_only,
	float width = -FLT_MIN
) {
	bool changed{ false };
	ImGui::SetNextItemWidth(width);

	if (ImGui::BeginCombo(label, GetRegisteredSequenceItemDescriptor(action.kind).label)) {
		auto draw_action = [&](RegisteredSequenceItemKind kind) {
			const auto& descriptor{ GetRegisteredSequenceItemDescriptor(kind) };
			if (timed_only && !descriptor.supports_timed) {
				return;
			}

			const bool selected{ action.kind == kind };
			if (ImGui::MenuItem(descriptor.label, nullptr, selected)) {
				action = MakeRegisteredSequenceItem(kind);
				changed = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s\n%s", descriptor.description, descriptor.key);
			}
		};

		if (!timed_only && ImGui::BeginMenu("Entity")) {
			draw_action(RegisteredSequenceItemKind::SpawnEntity);
			ImGui::Separator();
			draw_action(RegisteredSequenceItemKind::AddComponent);
			draw_action(RegisteredSequenceItemKind::RemoveComponent);
			ImGui::Separator();
			draw_action(RegisteredSequenceItemKind::SetVisible);
			ImGui::EndMenu();
		}

		constexpr std::array groups{ "Transform", "Audio", "Physics", "Game" };
		for (const char* group : groups) {
			const bool has_entries{ std::ranges::any_of(
				kRegisteredSequenceItemRegistry, [group, timed_only](const auto& descriptor) {
					return std::strcmp(descriptor.group, group) == 0 &&
						   (!timed_only || descriptor.supports_timed);
				}
			) };

			if (!has_entries || !ImGui::BeginMenu(group)) {
				continue;
			}

			for (const auto& descriptor : kRegisteredSequenceItemRegistry) {
				if (std::strcmp(descriptor.group, group) != 0 ||
					(timed_only && !descriptor.supports_timed)) {
					continue;
				}
				draw_action(descriptor.kind);
			}

			ImGui::EndMenu();
		}

		ImGui::EndCombo();
	}

	return changed;
}

bool SupportsInlineAddButton(RegisteredSequenceItemKind kind) {
	return kind == RegisteredSequenceItemKind::AddComponent;
}

bool DrawLifecycleCallbackCompact(
	LifecycleCallbackDefinition& callback, const PrefabRegistry& prefabs
) {
	bool remove{ false };
	float callback_left_screen_x{ ImGui::GetCursorScreenPos().x };

	ImGui::PushID(static_cast<int>(callback.id));

	const float remove_width{ ImGui::GetFrameHeight() };

	if (ImGui::BeginTable("LifecycleCallbackRow", 5, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthFixed, 126.0f);
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 94.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		callback_left_screen_x = ImGui::GetCursorScreenPos().x;
		DrawLifecycleEventCombo("##Event", callback.event);
		DrawItemTooltip("Lifecycle event that invokes this callback.");

		ImGui::TableSetColumnIndex(1);
		DrawEnumCombo("##CallbackKind", callback.kind, kLifecycleCallbackKindNames);

		ImGui::TableSetColumnIndex(2);

		switch (callback.kind) {
			case LifecycleCallbackKind::Item: {
				const bool show_add_button{ SupportsInlineAddButton(callback.registered_item.kind) };
				const float add_width{ ImGui::GetFrameHeight() };
				const float spacing{ ImGui::GetStyle().ItemSpacing.x };
				const float picker_width{
					show_add_button
						? std::max(1.0f, ImGui::GetContentRegionAvail().x - add_width - spacing)
						: -FLT_MIN
				};

				DrawRegisteredSequenceItemPicker("##CallbackItem", callback.registered_item, false, picker_width);

				if (show_add_button && callback.registered_item.kind == RegisteredSequenceItemKind::AddComponent) {
					ImGui::SameLine(0.0f, spacing);
					DrawAddComponentButton(callback.registered_item);
				}
				break;
			}
			case LifecycleCallbackKind::EmitEvent:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint(
					"##CallbackEvent", "Event name", callback.event_name.Data(),
					callback.event_name.Size()
				);
				DrawItemTooltip("Unique event_name emitted when this lifecycle event occurs.");
				break;
		}

		ImGui::TableSetColumnIndex(3);
		ImGui::Checkbox("##Enabled", &callback.enabled);

		ImGui::TableSetColumnIndex(4);
		if (ImGui::Button("x", ImVec2{ remove_width, ImGui::GetFrameHeight() })) {
			remove = true;
		}

		ImGui::EndTable();
	}

	if (callback.kind == LifecycleCallbackKind::Item) {
		DrawRegisteredSequenceItemParametersCompact(callback.registered_item, callback_left_screen_x, prefabs);
	}

	ImGui::PopID();
	return remove;
}

bool DrawAddableSectionHeader(
	const char* id, const char* label, bool default_open, bool empty, const char* section_tooltip,
	const char* empty_tooltip, const char* add_tooltip, bool& add_requested
) {
	static std::unordered_map<ImGuiID, bool> force_open_next_frame;

	ImGui::PushID(id);
	const ImGuiID tree_id{ ImGui::GetID("Tree") };

	if (force_open_next_frame[tree_id]) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		force_open_next_frame[tree_id] = false;
	}

	bool open{ false };
	const float add_width{ ImGui::GetFrameHeight() };

	if (ImGui::BeginTable("SectionHeaderRow", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, add_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Framed };
		if (default_open) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		open = ImGui::TreeNodeEx("Tree", flags, "%s", label);

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", empty ? empty_tooltip : section_tooltip);
		}

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("+", ImVec2{ add_width, ImGui::GetFrameHeight() })) {
			add_requested				   = true;
			open						   = true;
			force_open_next_frame[tree_id] = true;
		}
		DrawItemTooltip(add_tooltip);
		ImGui::EndTable();
	}

	ImGui::PopID();
	return open;
}

void DrawLifecycleSection(ScriptSequenceDefinition& sequence, const PrefabRegistry& prefabs) {
	char lifecycle_label[96]{};
	std::snprintf(
		lifecycle_label, sizeof(lifecycle_label), "Lifecycle (%zu)%s",
		sequence.lifecycle_callbacks.size(),
		sequence.destroy_on_complete ? "  [Destroy on Complete]" : ""
	);

	bool add_callback{ false };
	const bool lifecycle_open{ DrawAddableSectionHeader(
		"LifecycleSection", lifecycle_label, false, sequence.lifecycle_callbacks.empty(),
		"Optional lifecycle callbacks and completion cleanup.", "No lifecycle callbacks.",
		"Add a lifecycle callback.", add_callback
	) };

	if (add_callback) {
		sequence.lifecycle_callbacks.emplace_back();
	}

	if (!lifecycle_open) {
		return;
	}

	int remove_callback{ -1 };

	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_callbacks.size()); ++i) {
		if (DrawLifecycleCallbackCompact(
				sequence.lifecycle_callbacks[static_cast<std::size_t>(i)], prefabs
			)) {
			remove_callback = i;
		}
	}

	if (remove_callback >= 0) {
		sequence.lifecycle_callbacks.erase(sequence.lifecycle_callbacks.begin() + remove_callback);
	}
}

bool DrawTriggerCompact(TriggerDefinition& trigger, bool stop_trigger = false) {
	bool remove{ false };

	ImGui::PushID(static_cast<int>(trigger.id));

	const float remove_width{ ImGui::GetFrameHeight() };

	if (ImGui::BeginTable("TriggerRow", 4, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		DrawTriggerKindCombo("##Kind", trigger.kind);

		ImGui::TableSetColumnIndex(1);

		switch (trigger.kind) {
			case TriggerKind::OnCreate: {
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("Delay:");
				ImGui::SameLine();
				DrawDurationInput(
					"##Delay", trigger.duration_ms, -FLT_MIN,
					stop_trigger ? "Delay before the sequence stops after the entity is created."
								 : "Delay before the sequence starts after the entity is created."
				);
				break;
			}

			case TriggerKind::NamedEvent:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Event", trigger.event_name.Data(), trigger.event_name.Size());
				DrawItemTooltip("Stable name of the event to listen for.");
				break;

			case TriggerKind::KeyPressed:
			case TriggerKind::KeyReleased:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Key", trigger.key.Data(), trigger.key.Size());
				break;

			case TriggerKind::KeyHeld: {
				if (ImGui::BeginTable("HeldKey", 2, ImGuiTableFlags_SizingStretchProp)) {
					ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 92.0f);
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputText("##Key", trigger.key.Data(), trigger.key.Size());

					ImGui::TableSetColumnIndex(1);
					DrawDurationInput(
						"##HeldDuration", trigger.duration_ms, -FLT_MIN,
						"How long the key must be held."
					);
					ImGui::EndTable();
				}
				break;
			}

			case TriggerKind::MousePressed:
			case TriggerKind::MouseReleased:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText(
					"##MouseButton", trigger.mouse_button.Data(), trigger.mouse_button.Size()
				);
				break;

			case TriggerKind::MouseHeld: {
				if (ImGui::BeginTable("HeldMouse", 2, ImGuiTableFlags_SizingStretchProp)) {
					ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 92.0f);
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputText(
						"##MouseButton", trigger.mouse_button.Data(), trigger.mouse_button.Size()
					);

					ImGui::TableSetColumnIndex(1);
					DrawDurationInput(
						"##HeldDuration", trigger.duration_ms, -FLT_MIN,
						"How long the mouse button must be held."
					);
					ImGui::EndTable();
				}
				break;
			}

			case TriggerKind::OverlapStart:
			case TriggerKind::OverlapStop:
			case TriggerKind::CollisionStart:
			case TriggerKind::CollisionStop:  {
				const float spacing{ ImGui::GetStyle().ItemSpacing.x };
				const float available{ ImGui::GetContentRegionAvail().x };
				const float tag_width{ std::max(80.0f, (available - spacing) * 0.58f) };

				ImGui::SetNextItemWidth(tag_width);
				ImGui::InputTextWithHint(
					"##Tags", "Tags: Player,-Enemy", trigger.tag_filter.Data(),
					trigger.tag_filter.Size()
				);
				if (ImGui::IsItemHovered()) {
					const std::string tooltip{ BuildCommaSeparatedTooltip(
						"Comma-separated tags. Prefix a tag with '-' to exclude it.",
						trigger.tag_filter.View()
					) };
					ImGui::SetTooltip("%s", tooltip.c_str());
				}

				ImGui::SameLine(0.0f, spacing);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint(
					"##Masks", "Masks: 1,4,-8", trigger.mask_filter.Data(),
					trigger.mask_filter.Size()
				);
				if (ImGui::IsItemHovered()) {
					const std::string tooltip{ BuildCommaSeparatedTooltip(
						"Comma-separated integer masks. Positive values include; '-' excludes.",
						trigger.mask_filter.View()
					) };
					ImGui::SetTooltip("%s", tooltip.c_str());
				}
				break;
			}
		}

		ImGui::TableSetColumnIndex(2);
		ImGui::Checkbox("##Enabled", &trigger.enabled);

		ImGui::TableSetColumnIndex(3);
		if (ImGui::Button("x", ImVec2{ remove_width, ImGui::GetFrameHeight() })) {
			remove = true;
		}

		ImGui::EndTable();
	}

	ImGui::PopID();
	return remove;
}

void AddTrigger(std::vector<TriggerDefinition>& triggers, bool stop_triggers) {
	TriggerDefinition trigger;
	trigger.kind		= stop_triggers ? TriggerKind::NamedEvent : TriggerKind::OnCreate;
	trigger.duration_ms = 0.0f;
	if (stop_triggers) {
		trigger.event_name.Assign("sequence.stop");
	}
	triggers.push_back(std::move(trigger));
}

void DrawTriggerList(std::vector<TriggerDefinition>& triggers, bool stop_triggers) {
	int remove_trigger{ -1 };

	for (int i{ 0 }; i < static_cast<int>(triggers.size()); ++i) {
		if (DrawTriggerCompact(triggers[static_cast<std::size_t>(i)], stop_triggers)) {
			remove_trigger = i;
		}
	}

	if (remove_trigger >= 0) {
		triggers.erase(triggers.begin() + remove_trigger);
	}
}

void DrawTriggerSection(std::vector<TriggerDefinition>& triggers, bool stop_triggers) {
	char label[64]{};
	std::snprintf(
		label, sizeof(label), stop_triggers ? "Stop Triggers (%zu)" : "Start Triggers (%zu)",
		triggers.size()
	);

	bool add_trigger{ false };
	const bool open{ DrawAddableSectionHeader(
		stop_triggers ? "StopTriggerSection" : "StartTriggerSection", label, true, triggers.empty(),
		stop_triggers ? "Triggers that stop this sequence." : "Triggers that start this sequence.",
		stop_triggers ? "No stop triggers: this sequence only stops manually or on completion."
					  : "No start triggers: this sequence is started manually.",
		stop_triggers ? "Add a stop trigger." : "Add a start trigger.", add_trigger
	) };

	if (add_trigger) {
		AddTrigger(triggers, stop_triggers);
	}

	if (open) {
		DrawTriggerList(triggers, stop_triggers);
	}
}

struct SequenceDragPayload { int index; };

bool DrawSequenceItemCompact(
	SequenceItem& item,
	int index,
	bool active,
	float progress,
	bool& duplicate,
	int& move_from,
	int& move_to,
	const PrefabRegistry& prefabs
) {
	bool remove{ false };

	ImGui::PushID(static_cast<int>(item.id));

	const float drag_width{ 28.0f };
	const float type_width{ 108.0f };
	const float duration_width{ 76.0f };
	const float repeats_width{ GetCountControlWidth("Repeats") };
	const float add_width{ ImGui::GetFrameHeight() };
	const float remove_width{ ImGui::GetFrameHeight() };

	const SequenceItemKind displayed_kind{ item.kind };
	bool show_add_button{ false };

	if (displayed_kind == SequenceItemKind::Item) {
		show_add_button = SupportsInlineAddButton(std::get<InstantSequenceItem>(item.data).registered_item.kind);
	}

	int column_count{ 4 };
	if (displayed_kind == SequenceItemKind::Item && show_add_button) {
		++column_count;
	} else if (displayed_kind == SequenceItemKind::TimedItem) {
		column_count = 6;
	}

	const int remove_column{ column_count - 1 };
	float type_left_screen_x{ ImGui::GetCursorScreenPos().x };
	SequenceItemKind requested_kind{ item.kind };
	bool kind_changed{ false };

	if (ImGui::BeginTable("SequenceRow", column_count, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Drag", ImGuiTableColumnFlags_WidthFixed, drag_width);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, type_width);

		switch (displayed_kind) {
			case SequenceItemKind::Item:
				ImGui::TableSetupColumn("Registered Item", ImGuiTableColumnFlags_WidthStretch);
				if (show_add_button) {
					ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, add_width);
				}
				break;

			case SequenceItemKind::TimedItem:
				ImGui::TableSetupColumn(
					"Duration", ImGuiTableColumnFlags_WidthFixed, duration_width
				);
				ImGui::TableSetupColumn("Registered Item", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Repeats", ImGuiTableColumnFlags_WidthFixed, repeats_width);
				break;

			case SequenceItemKind::Wait:
			case SequenceItemKind::EmitEvent:
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
				break;
		}

		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		if (!item.enabled) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
		}
		ImGui::Button("::", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() });
		if (!item.enabled) {
			ImGui::PopStyleVar();
		}
		DrawItemTooltip(
			item.enabled ? "Right-click for options. Drag to reorder."
						 : "Disabled. Right-click for options or drag to reorder."
		);

		if (ImGui::BeginPopupContextItem("ItemMenu")) {
			if (ImGui::MenuItem(item.enabled ? "Disable" : "Enable")) {
				item.enabled = !item.enabled;
			}

			if (ImGui::MenuItem("Duplicate")) {
				duplicate = true;
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const SequenceDragPayload payload{ index };
			ImGui::SetDragDropPayload("PTGN_SEQUENCE_ITEM", &payload, sizeof(payload));
			ImGui::Text("%d. %s", index + 1, SequenceItemSummary(item).c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("PTGN_SEQUENCE_ITEM") }) {
				const auto* drag{ static_cast<const SequenceDragPayload*>(payload->Data) };

				if (drag) {
					move_from = drag->index;
					move_to = index;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::TableSetColumnIndex(1);
		type_left_screen_x = ImGui::GetCursorScreenPos().x;
		kind_changed	   = DrawEnumCombo("##Type", requested_kind, kSequenceItemNames);

		switch (displayed_kind) {
			case SequenceItemKind::Item: {
				auto& action_item{ std::get<InstantSequenceItem>(item.data) };

				ImGui::TableSetColumnIndex(2);
				DrawRegisteredSequenceItemPicker("##RegisteredItem", action_item.registered_item, false);

				if (show_add_button) {
					ImGui::TableSetColumnIndex(3);
					if (action_item.registered_item.kind == RegisteredSequenceItemKind::AddComponent) {
						DrawAddComponentButton(action_item.registered_item);
					}
				}
				break;
			}

			case SequenceItemKind::TimedItem: {
				auto& timed{ std::get<TimedSequenceItem>(item.data) };

				ImGui::TableSetColumnIndex(2);
				DrawDurationInput(
					"##Duration", timed.duration_ms, -FLT_MIN,
					"Duration of each timed sequence-item cycle."
				);

				ImGui::TableSetColumnIndex(3);
				DrawRegisteredSequenceItemPicker("##RegisteredItem", timed.registered_item, true);

				ImGui::TableSetColumnIndex(4);
				DrawCountControl(
					"Repeats", timed.additional_repeats, 0, 100, timed.infinite_repeats,
					"Additional full-duration cycles. Each repeat runs the timed item for the "
					"complete duration again."
				);
				break;
			}

			case SequenceItemKind::Wait: {
				ImGui::TableSetColumnIndex(2);
				auto& wait{ std::get<WaitItem>(item.data) };
				DrawDurationInput("##Duration", wait.duration_ms, -FLT_MIN, "Wait duration.");
				break;
			}

			case SequenceItemKind::EmitEvent: {
				ImGui::TableSetColumnIndex(2);
				auto& emit{ std::get<EmitEventItem>(item.data) };
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Event", emit.event_name.Data(), emit.event_name.Size());
				DrawItemTooltip("Stable name of the event to emit.");
				break;
			}
		}

		ImGui::TableSetColumnIndex(remove_column);
		if (ImGui::Button("x", ImVec2{ remove_width, ImGui::GetFrameHeight() })) {
			remove = true;
		}
		DrawItemTooltip("Delete this sequence item.");

		ImGui::EndTable();
	}

	if (kind_changed) {
		SetSequenceItemKind(item, requested_kind);
	}

	if (item.kind == SequenceItemKind::TimedItem) {
		auto& timed{ std::get<TimedSequenceItem>(item.data) };
		const float right_screen_x{ ImGui::GetWindowPos().x +
									ImGui::GetWindowContentRegionMax().x };
		const float available_width{ std::max(1.0f, right_screen_x - type_left_screen_x) };

		ImGui::SetCursorScreenPos(ImVec2{ type_left_screen_x, ImGui::GetCursorScreenPos().y });
		if (ImGui::BeginTable(
				"TimedItemOptionsRow", 2, ImGuiTableFlags_SizingStretchSame,
				ImVec2{ available_width, 0.0f }
			)) {
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

			ImGui::TableSetColumnIndex(0);
			DrawEnumCombo("##Ease", timed.ease, kEaseNames);

			ImGui::TableSetColumnIndex(1);
			DrawTimedFlagsCombo(timed);
			ImGui::EndTable();
		}
	}

	if (active) {
		ImGui::ProgressBar(progress, ImVec2{ -FLT_MIN, 2.0f }, "");
	}

	if (item.kind == SequenceItemKind::TimedItem) {
		DrawRegisteredSequenceItemParametersCompact(
			std::get<TimedSequenceItem>(item.data).registered_item, type_left_screen_x, prefabs
		);
	} else if (item.kind == SequenceItemKind::Item) {
		DrawRegisteredSequenceItemParametersCompact(
			std::get<InstantSequenceItem>(item.data).registered_item, type_left_screen_x, prefabs
		);
	}

	ImGui::PopID();
	return remove;
}

void DrawScriptSequenceItems(
	ScriptSequenceDefinition& sequence,
	ScriptSequence& binding,
	const PrefabRegistry& prefabs
) {
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int i{ 0 }; i < static_cast<int>(sequence.sequence.size()); ++i) {
		auto& item{ sequence.sequence[static_cast<std::size_t>(i)] };
		const bool active{
			binding.runtime.running &&
			binding.runtime.item_index == static_cast<std::size_t>(i)
		};
		bool duplicate{ false };

		if (DrawSequenceItemCompact(
				item,
				i,
				active,
				active ? GetRuntimeProgress(binding, sequence) : 0.0f,
				duplicate,
				move_from,
				move_to,
				prefabs
			)) {
			remove_index = i;
		}

		if (duplicate) {
			duplicate_index = i;
		}
	}

	if (move_from >= 0 && move_to >= 0) {
		MoveItem(sequence.sequence, move_from, move_to);
	}

	if (duplicate_index >= 0) {
		auto copy{ sequence.sequence[static_cast<std::size_t>(duplicate_index)] };
		copy.id = NextId();
		sequence.sequence.insert(
			sequence.sequence.begin() + duplicate_index + 1,
			std::move(copy)
		);
	}

	if (remove_index >= 0) {
		sequence.sequence.erase(sequence.sequence.begin() + remove_index);
		binding.runtime = {};
	}

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float button_width{
		(ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f
	};

	if (ImGui::Button("+ Item", ImVec2{ button_width, 0.0f })) {
		sequence.sequence.push_back(MakeSequenceItem(SequenceItemKind::Item));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Timed Item", ImVec2{ button_width, 0.0f })) {
		sequence.sequence.push_back(MakeSequenceItem(SequenceItemKind::TimedItem));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Wait", ImVec2{ button_width, 0.0f })) {
		sequence.sequence.push_back(MakeSequenceItem(SequenceItemKind::Wait));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Emit Event", ImVec2{ button_width, 0.0f })) {
		sequence.sequence.push_back(MakeSequenceItem(SequenceItemKind::EmitEvent));
	}
}

void PromoteBindingToGlobal(ScriptSequence& binding, GlobalScriptSequenceRegistry& registry) {
	if (binding.global_reference) {
		return;
	}

	if (auto* existing{ registry.FindByName(binding.local_definition.name.View()) }) {
		binding.global_reference   = true;
		binding.global_sequence_id = existing->id;
		binding.local_definition   = {};
		binding.runtime			   = {};
		return;
	}

	ScriptSequenceDefinition global{ std::move(binding.local_definition) };
	const Id id{ global.id };
	registry.definitions.push_back(std::move(global));
	binding.global_reference = true;
	binding.global_sequence_id = id;
	binding.local_definition = {};
	binding.runtime = {};
}

void DetachBindingToLocal(ScriptSequence& binding, GlobalScriptSequenceRegistry& registry) {
	if (!binding.global_reference) {
		return;
	}
	const auto* global{ registry.Find(binding.global_sequence_id) };
	binding.local_definition   = global ? CloneScriptSequenceDefinition(*global) : ScriptSequenceDefinition{};
	binding.global_reference = false;
	binding.global_sequence_id = 0;
	binding.runtime = {};
}

void DrawRuntimeButtons(
	EntityData& entity,
	ScriptSequence& binding,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context
) {
	if (ImGui::BeginTable("RuntimeButtons", 3, ImGuiTableFlags_SizingStretchSame)) {
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		if (ImGui::Button(
				binding.runtime.running ? "Restart" : "Start",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			StartScriptSequence(entity, binding, registry, context);
		}

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginDisabled(!binding.runtime.running);
		if (ImGui::Button(
				binding.runtime.paused ? "Resume" : "Pause",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			if (const auto* sequence{ ResolveScriptSequence(binding, registry) }) {
				SetScriptSequencePaused(entity, binding, *sequence, context, !binding.runtime.paused);
			}
		}
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(2);
		if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
			if (const auto* sequence{ ResolveScriptSequence(binding, registry) }) {
				StopScriptSequence(entity, binding, *sequence, context);
			} else {
				binding.runtime = {};
			}
		}

		ImGui::EndTable();
	}
}

bool DrawScriptSequence(
	EntityData& entity,
	ScriptSequence& binding,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context,
	const PrefabRegistry& prefabs
) {
	auto* sequence{ ResolveScriptSequence(binding, registry) };

	if (!sequence) {
		ImGui::TextDisabled("Missing global sequence");
		return false;
	}

	bool remove{ false };
	ImGui::PushID(static_cast<int>(binding.id));

	char header[192]{};
	std::snprintf(
		header, sizeof(header), "%s%s%s%s", binding.runtime.running ? "> " : "",
		binding.enabled ? "" : "[Disabled] ", binding.global_reference ? "[Global] " : "",
		sequence->name.Data()
	);

	const bool open{ ImGui::TreeNodeEx(
		"##Script Sequence",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
		"%s", header
	) };

	if (ImGui::BeginPopupContextItem("Script SequenceContextMenu")) {
		if (ImGui::MenuItem(binding.enabled ? "Disable" : "Enable")) {
			binding.enabled = !binding.enabled;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Delete")) {
			remove = true;
		}
		ImGui::EndPopup();
	}

	if (open) {
		if (ImGui::BeginTable("Script SequenceMainRow", 6, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 38.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn("Play", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableSetupColumn("Pause", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableSetupColumn("Padding", ImGuiTableColumnFlags_WidthFixed, 0.0f);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("Name");

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##Name", sequence->name.Data(), sequence->name.Size());

			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button(
					binding.runtime.running ? "Restart" : "Start",
					ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
				)) {
				StartScriptSequence(entity, binding, registry, context);
			}

			ImGui::TableSetColumnIndex(3);
			ImGui::BeginDisabled(!binding.runtime.running);
			if (ImGui::Button(
					binding.runtime.paused ? "Resume" : "Pause",
					ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
				)) {
				SetScriptSequencePaused(entity, binding, *sequence, context, !binding.runtime.paused);
			}
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(4);
			if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
				StopScriptSequence(entity, binding, *sequence, context);
			}

			ImGui::EndTable();
		}

		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("On Retrigger");
		ImGui::SameLine();

		const auto& style{ ImGui::GetStyle() };
		const float reentry_checkbox_spacing{ 8.0f };
		const float global_width{ ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
								  ImGui::CalcTextSize("Global").x };
		const float destroy_width{ ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
								   ImGui::CalcTextSize("Destroy on Complete").x };
		const float reentry_width{ std::max(
			1.0f, ImGui::GetContentRegionAvail().x - global_width - destroy_width -
					  reentry_checkbox_spacing - style.ItemSpacing.x - 30.0f
		) };

		DrawEnumCombo("##Reentry", sequence->reentry, kReentryNames, reentry_width);
		DrawItemTooltip(
			"Controls what happens if this sequence is triggered while already running."
		);

		ImGui::SameLine(0.0f, reentry_checkbox_spacing);
		bool global{ binding.global_reference };
		if (ImGui::Checkbox("Global", &global)) {
			if (global) {
				PromoteBindingToGlobal(binding, registry);
			} else {
				DetachBindingToLocal(binding, registry);
			}
			sequence = ResolveScriptSequence(binding, registry);
		}
		DrawItemTooltip(
			binding.global_reference ? "Shared sequence definition used by multiple entities."
									 : "Local sequence definition owned by this entity."
		);

		ImGui::SameLine();
		ImGui::Checkbox("Destroy on Complete", &sequence->destroy_on_complete);
		DrawItemTooltip(
			"Destroys the transient sequence runtime after completion. The owning entity and "
			"sequence definition remain."
		);

		DrawLifecycleSection(*sequence, prefabs);

		DrawTriggerSection(sequence->triggers, false);
		DrawTriggerSection(sequence->stop_triggers, true);

		const ImVec2 trigger_position{ ImGui::GetCursorScreenPos() };
		ImGui::SetCursorScreenPos(ImVec2{ trigger_position.x, trigger_position.y + 2.0f });

		char sequence_label[64]{};
		std::snprintf(sequence_label, sizeof(sequence_label), "Sequence (%zu)", sequence->sequence.size());

		const bool sequence_open{ ImGui::TreeNodeEx(
			"##Sequence",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Framed,
			"%s", sequence_label
		) };

		if (sequence_open) {
			DrawScriptSequenceItems(*sequence, binding, prefabs);
		}
	}

	ImGui::PopID();
	return remove;
}

ScriptSequence MakeLocalScriptSequence() {
	ScriptSequence binding;
	TriggerDefinition on_create;
	on_create.kind		  = TriggerKind::OnCreate;
	on_create.duration_ms = 0.0f;
	binding.local_definition.triggers.push_back(std::move(on_create));
	return binding;
}

ScriptSequence MakeGlobalScriptSequence(Id id) {
	ScriptSequence binding;
	binding.global_reference = true;
	binding.global_sequence_id = id;
	return binding;
}

void DrawAddScriptSequencePopup(
	ScriptsComponent& component,
	GlobalScriptSequenceRegistry& registry
) {
	if (!ImGui::BeginPopup("AddScript SequencePopup")) {
		return;
	}

	if (ImGui::MenuItem("Create New Script Sequence")) {
		component.sequences.push_back(MakeLocalScriptSequence());
	}

	if (ImGui::BeginMenu("Existing Script Sequence")) {
		if (registry.definitions.empty()) {
			ImGui::TextDisabled("No global scripts");
		}

		for (const auto& definition : registry.definitions) {
			const bool attached{ std::ranges::any_of(
				component.sequences, [&definition](const auto& binding) {
					return binding.global_reference && binding.global_sequence_id == definition.id;
				}
			) };

			ImGui::BeginDisabled(attached);

			if (ImGui::MenuItem(definition.name.Data())) {
				component.sequences.push_back(MakeGlobalScriptSequence(definition.id));
			}

			ImGui::EndDisabled();

			if (attached && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("Already attached to this entity");
			}
		}

		ImGui::EndMenu();
	}

	ImGui::EndPopup();
}

void DrawScriptsComponent(
	EntityData& entity,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context,
	const PrefabRegistry& prefabs
) {
	auto& component{ entity.entity.Get<ScriptsComponent>() };
	ImGui::PushID("ScriptsComponent");

	char header[96]{};
	std::snprintf(header, sizeof(header), "Scripts (%zu)", component.sequences.size());

	const bool open{ ImGui::TreeNodeEx(
		"##ScriptsComponent",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"%s", header
	) };

	bool remove_component{ false };
	if (ImGui::BeginPopupContextItem("ScriptsComponentContextMenu")) {
		if (ImGui::MenuItem("Delete Component")) {
			remove_component = true;
		}
		ImGui::EndPopup();
	}

	if (remove_component) {
		if (open) {
			ImGui::TreePop();
		}
		entity.entity.Remove<ScriptsComponent>();
		ImGui::PopID();
		return;
	}

	if (open) {
		int remove_index{ -1 };
		for (int i{ 0 }; i < static_cast<int>(component.sequences.size()); ++i) {
			if (DrawScriptSequence(
					entity,
					component.sequences[static_cast<std::size_t>(i)],
					registry,
					context,
					prefabs
				)) {
				remove_index = i;
			}
		}

		if (remove_index >= 0) {
			component.sequences.erase(component.sequences.begin() + remove_index);
		}

		const ImVec2 add_sequence_position{ ImGui::GetCursorScreenPos() };
		ImGui::SetCursorScreenPos(
			ImVec2{ add_sequence_position.x, add_sequence_position.y + 4.0f }
		);

		if (ImGui::Button("+ Add Script Sequence", ImVec2{ -FLT_MIN, 0.0f })) {
			ImGui::OpenPopup("AddScript SequencePopup");
		}
		DrawAddScriptSequencePopup(component, registry);

		const ImVec2 add_sequence_position2{ ImGui::GetCursorScreenPos() };
		ImGui::SetCursorScreenPos(
			ImVec2{ add_sequence_position2.x, add_sequence_position2.y + 4.0f }
		);

		char activity_label[64]{};
		std::snprintf(activity_label, sizeof(activity_label), "Demo Activity (%zu)", context.activity.size());

		const bool activity_open{ ImGui::TreeNodeEx(
			"##DemoActivity",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
			"%s", activity_label
		) };

		if (activity_open) {
			if (ImGui::BeginChild("ActivityLog", ImVec2{ 0.0f, 150.0f }, true)) {
				const bool was_at_bottom{
					ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f
				};

				if (context.activity.empty()) {
					ImGui::TextDisabled("No activity yet.");
				}

				for (const auto& entry : context.activity) {
					ImGui::TextUnformatted(entry.text.c_str());
				}

				if (was_at_bottom) {
					ImGui::SetScrollHereY(1.0f);
				}
			}
			ImGui::EndChild();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void DrawInspector(
	EntityData& entity, GlobalScriptSequenceRegistry& registry, DemoRuntimeContext& context,
	const PrefabRegistry& prefabs
) {
	ImGui::TextDisabled("Entity");
	if (ImGui::BeginTable("EntityIdentity", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Name", "Entity name", entity.name.Data(), entity.name.Size());

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Tag", "Tag", entity.tag.Data(), entity.tag.Size());
		ImGui::EndTable();
	}

	if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
		ImGui::DragFloat2("Position", entity.position, 1.0f, -100000.0f, 100000.0f, "%.0f");
		ImGui::DragFloat("Rotation", &entity.rotation, 1.0f, -3600.0f, 3600.0f, "%.1f deg");
		ImGui::DragFloat2("Scale", entity.scale, 0.01f, -1000.0f, 1000.0f, "%.2f");
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Visible", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
		ImGui::Checkbox("Value", &entity.visible);
		ImGui::TreePop();
	}
	if (entity.entity.Has<ScriptsComponent>()) {
		DrawScriptsComponent(entity, registry, context, prefabs);
	}

	if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}
	if (ImGui::BeginPopup("AddComponentPopup")) {
		ImGui::BeginDisabled(entity.entity.Has<ScriptsComponent>());
		if (ImGui::MenuItem("Scripts")) {
			entity.entity.Add<ScriptsComponent>();
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}
}


std::string MakePrefabKey(std::string_view name) {
	std::string key{ "prefabs/" };
	bool previous_separator{ false };

	for (unsigned char c : name) {
		if (std::isalnum(c)) {
			key.push_back(static_cast<char>(std::tolower(c)));
			previous_separator = false;
		} else if (!previous_separator && key.size() > std::string_view{ "prefabs/" }.size()) {
			key.push_back('_');
			previous_separator = true;
		}
	}

	while (!key.empty() && key.back() == '_') {
		key.pop_back();
	}

	if (key == "prefabs/") {
		key += "new_entity";
	}

	return key;
}

std::string MakeUniquePrefabKey(const PrefabRegistry& prefabs, std::string base) {
	if (!prefabs.FindByKey(base)) {
		return base;
	}

	for (int suffix{ 2 }; suffix < 10000; ++suffix) {
		std::string candidate{ base + "_" + std::to_string(suffix) };
		if (!prefabs.FindByKey(candidate)) {
			return candidate;
		}
	}

	return base + "_copy";
}

bool HasPrefabComponent(const PrefabDefinition& prefab, ComponentKind kind) {
	return std::ranges::any_of(prefab.components, [kind](const auto& component) {
		return component.kind == kind;
	});
}

PrefabDefinition MakeNewPrefab(const PrefabRegistry& prefabs) {
	PrefabDefinition prefab;
	prefab.name.Assign("New Prefab");
	prefab.key.Assign(MakeUniquePrefabKey(prefabs, "prefabs/new_entity"));
	prefab.components.push_back(MakeComponent(ComponentKind::Transform));
	prefab.components.push_back(MakeComponent(ComponentKind::Visible));
	return prefab;
}

PrefabDefinition MakePrefabFromEntity(const EntityData& entity, const PrefabRegistry& prefabs) {
	PrefabDefinition prefab;
	prefab.name.Assign(entity.name.View());
	prefab.key.Assign(MakeUniquePrefabKey(prefabs, MakePrefabKey(entity.name.View())));
	prefab.tag.Assign(entity.tag.View());

	auto transform{ MakeComponent(ComponentKind::Transform) };
	auto& transform_data{ std::get<TransformComponentData>(transform.data) };
	transform_data.position[0] = entity.position[0];
	transform_data.position[1] = entity.position[1];
	transform_data.rotation = entity.rotation;
	transform_data.scale[0] = entity.scale[0];
	transform_data.scale[1] = entity.scale[1];
	prefab.components.push_back(std::move(transform));

	auto visible{ MakeComponent(ComponentKind::Visible) };
	std::get<VisibleComponentData>(visible.data).visible = entity.visible;
	prefab.components.push_back(std::move(visible));

	return prefab;
}

PrefabRegistry MakeDemoPrefabs() {
	PrefabRegistry prefabs;

	PrefabDefinition zombie;
	zombie.key.Assign("prefabs/zombie");
	zombie.name.Assign("Zombie");
	zombie.tag.Assign("Enemy");
	zombie.components.push_back(MakeComponent(ComponentKind::Transform));

	auto zombie_sprite{ MakeComponent(ComponentKind::Sprite) };
	auto& zombie_sprite_data{ std::get<SpriteComponentData>(zombie_sprite.data) };
	zombie_sprite_data.texture.Assign("textures/zombie.png");
	zombie_sprite_data.size[0] = 48.0f;
	zombie_sprite_data.size[1] = 64.0f;
	zombie.components.push_back(std::move(zombie_sprite));

	auto zombie_collider{ MakeComponent(ComponentKind::Collider) };
	auto& zombie_collider_data{ std::get<ColliderComponentData>(zombie_collider.data) };
	zombie_collider_data.mode = 2;
	zombie_collider_data.radius = 18.0f;
	zombie_collider_data.mask = 4;
	zombie.components.push_back(std::move(zombie_collider));

	auto zombie_health{ MakeComponent(ComponentKind::Health) };
	auto& zombie_health_data{ std::get<HealthComponentData>(zombie_health.data) };
	zombie_health_data.maximum = 120.0f;
	zombie_health_data.current = 120.0f;
	zombie.components.push_back(std::move(zombie_health));
	zombie.components.push_back(MakeComponent(ComponentKind::Zombie));
	prefabs.definitions.push_back(std::move(zombie));

	PrefabDefinition projectile;
	projectile.key.Assign("prefabs/fireball_projectile");
	projectile.name.Assign("Fireball Projectile");
	projectile.tag.Assign("Projectile");
	projectile.components.push_back(MakeComponent(ComponentKind::Transform));

	auto projectile_sprite{ MakeComponent(ComponentKind::Sprite) };
	auto& projectile_sprite_data{ std::get<SpriteComponentData>(projectile_sprite.data) };
	projectile_sprite_data.texture.Assign("textures/fireball.png");
	projectile_sprite_data.size[0] = 24.0f;
	projectile_sprite_data.size[1] = 24.0f;
	projectile.components.push_back(std::move(projectile_sprite));

	auto projectile_collider{ MakeComponent(ComponentKind::Collider) };
	auto& projectile_collider_data{ std::get<ColliderComponentData>(projectile_collider.data) };
	projectile_collider_data.mode = 3;
	projectile_collider_data.radius = 10.0f;
	projectile_collider_data.mask = 8;
	projectile.components.push_back(std::move(projectile_collider));

	auto projectile_damage{ MakeComponent(ComponentKind::Damage) };
	auto& projectile_damage_data{ std::get<DamageComponentData>(projectile_damage.data) };
	projectile_damage_data.amount = 25.0f;
	projectile_damage_data.damage_type.Assign("Fire");
	projectile.components.push_back(std::move(projectile_damage));

	auto projectile_lifetime{ MakeComponent(ComponentKind::Lifetime) };
	std::get<LifetimeComponentData>(projectile_lifetime.data).duration_ms = 2200.0f;
	projectile.components.push_back(std::move(projectile_lifetime));
	prefabs.definitions.push_back(std::move(projectile));

	PrefabDefinition pickup;
	pickup.key.Assign("prefabs/health_pickup");
	pickup.name.Assign("Health Pickup");
	pickup.tag.Assign("Pickup");
	pickup.components.push_back(MakeComponent(ComponentKind::Transform));

	auto pickup_sprite{ MakeComponent(ComponentKind::Sprite) };
	std::get<SpriteComponentData>(pickup_sprite.data).texture.Assign("textures/health_pickup.png");
	pickup.components.push_back(std::move(pickup_sprite));
	pickup.components.push_back(MakeComponent(ComponentKind::Collider));
	prefabs.definitions.push_back(std::move(pickup));

	return prefabs;
}

bool DrawPrefabComponent(ComponentDefinition& component) {
	bool remove{ false };
	float members_left_screen_x{ ImGui::GetCursorScreenPos().x };

	ImGui::PushID(static_cast<int>(component.id));

	bool open{ false };
	if (ImGui::BeginTable("PrefabComponentHeader", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		members_left_screen_x = ImGui::GetCursorScreenPos().x;
		open = ImGui::TreeNodeEx(
			"##Component",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
			"%s", GetComponentDescriptor(component.kind).label
		);
		DrawItemTooltip(GetComponentDescriptor(component.kind).description);

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
			remove = true;
		}

		ImGui::EndTable();
	}

	if (open) {
		DrawComponentMembers(component, members_left_screen_x);
	}

	ImGui::PopID();
	return remove;
}

void DrawPrefabInspector(PrefabDefinition& prefab) {
	ImGui::TextDisabled("Prefab Asset");

	if (ImGui::BeginTable("PrefabIdentity", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 145.0f);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##PrefabName", "Display name", prefab.name.Data(), prefab.name.Size());

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##PrefabTag", "Tag", prefab.tag.Data(), prefab.tag.Size());
		ImGui::EndTable();
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##PrefabKey", "Prefab key", prefab.key.Data(), prefab.key.Size());
	DrawItemTooltip("Stable asset key referenced by Spawn Entity actions.");

	char components_label[64]{};
	std::snprintf(components_label, sizeof(components_label), "Components (%zu)", prefab.components.size());

	const bool components_open{ ImGui::TreeNodeEx(
		"##PrefabComponents",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_NoTreePushOnOpen,
		"%s", components_label
	) };

	if (!components_open) {
		return;
	}

	int remove_component{ -1 };
	for (int i{ 0 }; i < static_cast<int>(prefab.components.size()); ++i) {
		if (DrawPrefabComponent(prefab.components[static_cast<std::size_t>(i)])) {
			remove_component = i;
		}
	}

	if (remove_component >= 0) {
		prefab.components.erase(prefab.components.begin() + remove_component);
	}

	if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddPrefabComponent");
	}

	if (ImGui::BeginPopup("AddPrefabComponent")) {
		constexpr std::array groups{ "Core", "Graphics", "Physics", "Gameplay" };

		for (const char* group : groups) {
			if (!ImGui::BeginMenu(group)) {
				continue;
			}

			for (const auto& descriptor : kComponentRegistry) {
				if (std::strcmp(descriptor.group, group) != 0) {
					continue;
				}

				const bool already_added{ HasPrefabComponent(prefab, descriptor.kind) };
				ImGui::BeginDisabled(already_added);

				if (ImGui::MenuItem(descriptor.label)) {
					prefab.components.push_back(MakeComponent(descriptor.kind));
				}

				ImGui::EndDisabled();

				if (already_added && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					ImGui::SetTooltip("Prefab already contains this component.");
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGui::TextDisabled(
		"These reflected values are serialized into the prefab asset and copied into each spawn."
	);
}

void DrawPrefabPanel(
	PrefabRegistry& prefabs, const std::vector<EntityData>& entities, int selected_entity,
	int& selected_prefab, bool& inspect_prefab
) {
	ImGui::TextDisabled("Prefabs");
	ImGui::Separator();

	int delete_index{ -1 };
	int duplicate_index{ -1 };

	for (int i{ 0 }; i < static_cast<int>(prefabs.definitions.size()); ++i) {
		auto& prefab{ prefabs.definitions[static_cast<std::size_t>(i)] };
		ImGui::PushID(static_cast<int>(prefab.id));

		if (ImGui::Selectable(
				prefab.name.Data(), inspect_prefab && selected_prefab == i, 0,
				ImVec2{ 0.0f, 25.0f }
			)) {
			selected_prefab = i;
			inspect_prefab = true;
		}
		DrawItemTooltip(prefab.key.Data());

		if (ImGui::BeginPopupContextItem("PrefabContext")) {
			if (ImGui::MenuItem("Duplicate")) {
				duplicate_index = i;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) {
				delete_index = i;
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	if (duplicate_index >= 0) {
		PrefabDefinition copy{ prefabs.definitions[static_cast<std::size_t>(duplicate_index)] };
		copy.id = NextId();
		copy.name.Assign(std::string{ copy.name.Data() } + " Copy");
		copy.key.Assign(MakeUniquePrefabKey(prefabs, std::string{ copy.key.Data() } + "_copy"));
		for (auto& component : copy.components) {
			component.id = NextId();
		}
		prefabs.definitions.insert(
			prefabs.definitions.begin() + duplicate_index + 1, std::move(copy)
		);
		selected_prefab = duplicate_index + 1;
		inspect_prefab = true;
	}

	if (delete_index >= 0) {
		prefabs.definitions.erase(prefabs.definitions.begin() + delete_index);

		if (prefabs.definitions.empty()) {
			selected_prefab = -1;
			inspect_prefab = false;
		} else {
			if (selected_prefab > delete_index) {
				--selected_prefab;
			}
			selected_prefab = std::clamp(
				selected_prefab, 0, static_cast<int>(prefabs.definitions.size()) - 1
			);
		}
	}

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float button_width{ (ImGui::GetContentRegionAvail().x - spacing) * 0.5f };

	if (ImGui::Button("+ New", ImVec2{ button_width, 0.0f })) {
		prefabs.definitions.push_back(MakeNewPrefab(prefabs));
		selected_prefab = static_cast<int>(prefabs.definitions.size()) - 1;
		inspect_prefab = true;
	}

	ImGui::SameLine();

	const bool has_selected_entity{
		selected_entity >= 0 && selected_entity < static_cast<int>(entities.size())
	};
	ImGui::BeginDisabled(!has_selected_entity);

	if (ImGui::Button("From Entity", ImVec2{ button_width, 0.0f })) {
		prefabs.definitions.push_back(
			MakePrefabFromEntity(
				entities[static_cast<std::size_t>(selected_entity)], prefabs
			)
		);
		selected_prefab = static_cast<int>(prefabs.definitions.size()) - 1;
		inspect_prefab = true;
	}

	ImGui::EndDisabled();
	DrawItemTooltip("Create a prefab from the selected entity's reflected demo components.");
}

void DrawPrefabPreview(const PrefabDefinition& prefab) {
	const ImVec2 start{ ImGui::GetCursorScreenPos() };
	const ImVec2 size{ ImGui::GetContentRegionAvail() };
	ImDrawList* draw{ ImGui::GetWindowDrawList() };

	draw->AddRectFilled(
		start, ImVec2{ start.x + size.x, start.y + size.y },
		ImGui::GetColorU32(ImGuiCol_FrameBg)
	);

	const float card_width{ std::min(440.0f, std::max(260.0f, size.x - 80.0f)) };
	const float card_height{ std::min(
		360.0f, 110.0f + static_cast<float>(prefab.components.size()) * 28.0f
	) };
	const ImVec2 card_min{
		start.x + (size.x - card_width) * 0.5f,
		start.y + (size.y - card_height) * 0.5f
	};
	const ImVec2 card_max{ card_min.x + card_width, card_min.y + card_height };

	draw->AddRectFilled(card_min, card_max, ImGui::GetColorU32(ImGuiCol_ChildBg), 6.0f);
	draw->AddRect(card_min, card_max, ImGui::GetColorU32(ImGuiCol_Border), 6.0f);
	draw->AddText(
		ImVec2{ card_min.x + 16.0f, card_min.y + 14.0f },
		ImGui::GetColorU32(ImGuiCol_Text), prefab.name.Data()
	);
	draw->AddText(
		ImVec2{ card_min.x + 16.0f, card_min.y + 36.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled), prefab.key.Data()
	);

	std::string tag_line{ "Tag: " };
	tag_line += prefab.tag.Empty() ? "(none)" : prefab.tag.Data();
	draw->AddText(
		ImVec2{ card_min.x + 16.0f, card_min.y + 58.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled), tag_line.c_str()
	);

	float y{ card_min.y + 92.0f };
	for (const auto& component : prefab.components) {
		const auto& descriptor{ GetComponentDescriptor(component.kind) };
		draw->AddRectFilled(
			ImVec2{ card_min.x + 16.0f, y - 3.0f },
			ImVec2{ card_max.x - 16.0f, y + 20.0f },
			ImGui::GetColorU32(ImGuiCol_Button), 3.0f
		);
		draw->AddText(
			ImVec2{ card_min.x + 24.0f, y },
			ImGui::GetColorU32(ImGuiCol_Text), descriptor.label
		);
		y += 28.0f;
	}

	draw->AddText(
		ImVec2{ start.x + 10.0f, start.y + 10.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Prefab preview: this is serialized construction data, not a live ECS entity."
	);

	ImGui::InvisibleButton("PrefabCanvas", size);
}

std::vector<EntityData> MakeDemoEntities(
	ptgn::Manager& manager,
	GlobalScriptSequenceRegistry& registry
) {
	std::vector<EntityData> entities;

	// These builder calls create the exact same structures edited by the inspector.
	ScriptSequenceBuilder opened_indicator_builder{ "Door Opened Indicator" };
	opened_indicator_builder
		.Reentry(ReentryMode::Restart)
		.StartOn(MakeNamedEventTrigger("game.door_opened"))
		.StopOn(MakeNamedEventTrigger("game.door_closed"));

	opened_indicator_builder
		.During(350.0f, MakeMoveToItem("Self", 0.0f, 55.0f, true))
		.EaseWith(Ease::OutBack)
		.End();

	auto opened_indicator{ opened_indicator_builder.BuildDefinition() };
	const Id opened_indicator_id{ opened_indicator.id };
	registry.definitions.push_back(std::move(opened_indicator));

	ScriptSequenceBuilder closed_indicator_builder{ "Door Closed Indicator" };
	closed_indicator_builder
		.Reentry(ReentryMode::Restart)
		.StartOn(MakeNamedEventTrigger("game.door_closed"))
		.StopOn(MakeNamedEventTrigger("game.door_opened"));

	closed_indicator_builder
		.During(350.0f, MakeMoveToItem("Self", 300.0f, -110.0f, false))
		.EaseWith(Ease::OutCubic)
		.End();

	auto closed_indicator{ closed_indicator_builder.BuildDefinition() };
	const Id closed_indicator_id{ closed_indicator.id };
	registry.definitions.push_back(std::move(closed_indicator));

	EntityData player{ CreateEntityData(manager) };
	player.name.Assign("Player");
	player.tag.Assign("Player");
	player.position[0] = -330.0f;
	player.position[1] = 0.0f;
	player.entity.Add<DemoPlayer>();
	{
		auto& rectangle{ player.entity.Get<DemoRectangle>() };
		rectangle.size[0] = 38.0f;
		rectangle.size[1] = 38.0f;
		rectangle.color = IM_COL32(70, 150, 245, 255);
	}
	entities.push_back(std::move(player));

	EntityData sensor{ CreateEntityData(manager) };
	sensor.name.Assign("Door Sensor");
	sensor.tag.Assign("Door");
	sensor.position[0] = -70.0f;
	sensor.position[1] = 0.0f;
	sensor.entity.Add<DemoDoorSensor>();
	sensor.entity.Add<ScriptsComponent>();
	{
		auto& rectangle{ sensor.entity.Get<DemoRectangle>() };
		rectangle.size[0] = 110.0f;
		rectangle.size[1] = 170.0f;
		rectangle.color = IM_COL32(60, 190, 115, 80);
		rectangle.sensor = true;
	}

	ScriptSequenceBuilder open_builder{ "Open Sliding Panel" };
	open_builder
		.Reentry(ReentryMode::Restart)
		.StartOn(MakeOverlapTrigger(TriggerKind::OverlapStart, "Player"))
		.StopOn(MakeOverlapTrigger(TriggerKind::OverlapStop, "Player"));

	open_builder
		.During(500.0f, MakeMoveToItem("MovingPanel", 225.0f, 0.0f, false))
		.EaseWith(Ease::OutCubic)
		.End()
		.EmitEvent("game.door_opened");

	sensor.entity.Get<ScriptsComponent>().sequences.push_back(open_builder.Build());

	ScriptSequenceBuilder close_builder{ "Close Sliding Panel" };
	close_builder
		.Reentry(ReentryMode::Restart)
		.StartOn(MakeOverlapTrigger(TriggerKind::OverlapStop, "Player"))
		.StopOn(MakeOverlapTrigger(TriggerKind::OverlapStart, "Player"));

	close_builder
		.During(500.0f, MakeMoveToItem("MovingPanel", 70.0f, 0.0f, false))
		.EaseWith(Ease::OutCubic)
		.End()
		.EmitEvent("game.door_closed");

	sensor.entity.Get<ScriptsComponent>().sequences.push_back(close_builder.Build());
	entities.push_back(std::move(sensor));

	EntityData moving_panel{ CreateEntityData(manager) };
	moving_panel.name.Assign("Sliding Panel");
	moving_panel.tag.Assign("MovingPanel");
	moving_panel.position[0] = 70.0f;
	moving_panel.position[1] = 0.0f;
	moving_panel.entity.Add<DemoMovingPanel>();
	{
		auto& rectangle{ moving_panel.entity.Get<DemoRectangle>() };
		rectangle.size[0] = 62.0f;
		rectangle.size[1] = 170.0f;
		rectangle.color = IM_COL32(225, 145, 60, 255);
	}
	entities.push_back(std::move(moving_panel));

	EntityData indicator{ CreateEntityData(manager) };
	indicator.name.Assign("Event Indicator");
	indicator.tag.Assign("Indicator");
	indicator.position[0] = 300.0f;
	indicator.position[1] = -110.0f;
	indicator.entity.Add<ScriptsComponent>();
	indicator.entity.Get<ScriptsComponent>().sequences.push_back(
		MakeGlobalScriptSequence(opened_indicator_id)
	);
	indicator.entity.Get<ScriptsComponent>().sequences.push_back(
		MakeGlobalScriptSequence(closed_indicator_id)
	);
	{
		auto& rectangle{ indicator.entity.Get<DemoRectangle>() };
		rectangle.size[0] = 42.0f;
		rectangle.size[1] = 42.0f;
		rectangle.color = IM_COL32(235, 205, 70, 255);
	}
	entities.push_back(std::move(indicator));

	EntityData factory{ CreateEntityData(manager) };
	factory.name.Assign("Runtime Factory");
	factory.tag.Assign("Spawner");
	factory.position[0] = -300.0f;
	factory.position[1] = -180.0f;
	factory.entity.Add<ScriptsComponent>();
	{
		auto& rectangle{ factory.entity.Get<DemoRectangle>() };
		rectangle.size[0] = 120.0f;
		rectangle.size[1] = 42.0f;
		rectangle.color = IM_COL32(120, 105, 160, 255);
	}

	ScriptSequenceBuilder tools_builder{ "Runtime Entity Tools" };
	auto spawn_item{ MakeRegisteredSequenceItem(RegisteredSequenceItemKind::SpawnEntity) };
	auto& spawn_params{ std::get<SpawnEntityParams>(spawn_item.parameters) };
	spawn_params.prefab_key.Assign("prefabs/zombie");
	spawn_params.count = 3;
	spawn_params.area = SpawnArea::Circle;
	spawn_params.radius = 72.0f;
	tools_builder.Then(std::move(spawn_item));

	factory.entity.Get<ScriptsComponent>().sequences.push_back(tools_builder.Build());
	entities.push_back(std::move(factory));

	manager.Refresh();
	return entities;
}

void DrawHierarchy(
	ptgn::Manager& manager,
	std::vector<EntityData>& entities,
	int& selected_index,
	bool& inspect_prefab
) {
	ImGui::TextDisabled("Scene Hierarchy");
	ImGui::Separator();

	for (int i{ 0 }; i < static_cast<int>(entities.size()); ++i) {
		if (ImGui::Selectable(
				entities[static_cast<std::size_t>(i)].name.Data(),
				!inspect_prefab && selected_index == i, 0, ImVec2{ 0.0f, 25.0f }
			)) {
			selected_index = i;
			inspect_prefab = false;
		}
	}

	if (ImGui::Button("+ Entity", ImVec2{ -FLT_MIN, 0.0f })) {
		EntityData entity{ CreateEntityData(manager) };
		entity.name.Assign("New Entity");
		entities.push_back(std::move(entity));
		manager.Refresh();
		selected_index = static_cast<int>(entities.size()) - 1;
		inspect_prefab = false;
	}
}

void DrawSidebar(
	ptgn::Manager& manager,
	std::vector<EntityData>& entities,
	int& selected_entity,
	PrefabRegistry& prefabs,
	int& selected_prefab,
	bool& inspect_prefab
) {
	if (ImGui::BeginTabBar("SidebarTabs")) {
		if (ImGui::BeginTabItem("Scene")) {
			DrawHierarchy(manager, entities, selected_entity, inspect_prefab);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Prefabs")) {
			DrawPrefabPanel(
				prefabs, entities, selected_entity, selected_prefab, inspect_prefab
			);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void DrawSceneView(const std::vector<EntityData>& entities, int selected_index) {
	const ImVec2 start{ ImGui::GetCursorScreenPos() };
	const ImVec2 size{ ImGui::GetContentRegionAvail() };
	ImDrawList* draw{ ImGui::GetWindowDrawList() };

	draw->AddRectFilled(
		start,
		ImVec2{ start.x + size.x, start.y + size.y },
		ImGui::GetColorU32(ImGuiCol_FrameBg)
	);

	const ImVec2 center{ start.x + size.x * 0.5f, start.y + size.y * 0.5f };

	// Simple floor guide.
	draw->AddLine(
		ImVec2{ start.x + 20.0f, center.y },
		ImVec2{ start.x + size.x - 20.0f, center.y },
		ImGui::GetColorU32(ImGuiCol_Border)
	);

	for (int i{ 0 }; i < static_cast<int>(entities.size()); ++i) {
		const auto& entity{ entities[static_cast<std::size_t>(i)] };
		if (!entity.visible || !entity.entity.Has<DemoRectangle>()) {
			continue;
		}

		const auto& rectangle{ entity.entity.Get<DemoRectangle>() };
		const ImVec2 p{ center.x + entity.position[0], center.y - entity.position[1] };
		const ImVec2 half{
			rectangle.size[0] * 0.5f * std::abs(entity.scale[0]),
			rectangle.size[1] * 0.5f * std::abs(entity.scale[1])
		};

		const ImVec2 min{ p.x - half.x, p.y - half.y };
		const ImVec2 max{ p.x + half.x, p.y + half.y };

		if (rectangle.sensor) {
			draw->AddRectFilled(min, max, rectangle.color, 4.0f);
			draw->AddRect(
				min,
				max,
				IM_COL32(70, 230, 135, 220),
				4.0f,
				0,
				2.0f
			);
		} else {
			draw->AddRectFilled(min, max, rectangle.color, 4.0f);
		}

		if (i == selected_index) {
			draw->AddRect(
				ImVec2{ min.x - 3.0f, min.y - 3.0f },
				ImVec2{ max.x + 3.0f, max.y + 3.0f },
				ImGui::GetColorU32(ImGuiCol_ButtonHovered),
				5.0f,
				0,
				2.0f
			);
		}

		const ImVec2 text_size{ ImGui::CalcTextSize(entity.name.Data()) };
		draw->AddText(
			ImVec2{ p.x - text_size.x * 0.5f, max.y + 5.0f },
			ImGui::GetColorU32(ImGuiCol_Text),
			entity.name.Data()
		);
	}

	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 10.0f },
		ImGui::GetColorU32(ImGuiCol_Text),
		"WASD: move the Player"
	);

	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 30.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Enter the green Door Sensor to move the orange Sliding Panel aside. Leave it to close."
	);

	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 50.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Overlap events trigger the door sequences; emitted named events move the yellow indicator."
	);

	ImGui::InvisibleButton("SceneCanvas", size);
}

void DrawApplication(
	ptgn::Manager& manager,
	std::vector<EntityData>& entities,
	int& selected_entity,
	GlobalScriptSequenceRegistry& registry,
	DemoRuntimeContext& context,
	PrefabRegistry& prefabs,
	int& selected_prefab,
	bool& inspect_prefab
) {
	ImGuiViewport* viewport{ ImGui::GetMainViewport() };
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	constexpr ImGuiWindowFlags flags{
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
	};
	ImGui::Begin("Script Sequence Component Inspector Demo", nullptr, flags);

	if (ImGui::BeginTable(
			"Layout", 3,
			ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthFixed, 235.0f);
		ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 570.0f);

		ImGui::TableNextColumn();
		ImGui::BeginChild("HierarchyChild");
		DrawSidebar(
			manager, entities, selected_entity, prefabs, selected_prefab, inspect_prefab
		);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("SceneChild");

		const bool valid_prefab{
			inspect_prefab && selected_prefab >= 0 &&
			selected_prefab < static_cast<int>(prefabs.definitions.size())
		};

		if (valid_prefab) {
			DrawPrefabPreview(prefabs.definitions[static_cast<std::size_t>(selected_prefab)]);
		} else {
			DrawSceneView(entities, selected_entity);
		}

		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("InspectorChild");
		ImGui::TextDisabled(valid_prefab ? "Prefab Inspector" : "Inspector");
		ImGui::Separator();

		if (valid_prefab) {
			DrawPrefabInspector(
				prefabs.definitions[static_cast<std::size_t>(selected_prefab)]
			);
		} else if (
			selected_entity >= 0 && selected_entity < static_cast<int>(entities.size())
		) {
			DrawInspector(
				entities[static_cast<std::size_t>(selected_entity)], registry, context,
				prefabs
			);
		}

		ImGui::EndChild();
		ImGui::EndTable();
	}
	ImGui::End();
}

void ConfigureStyle() {
	ImGui::StyleColorsDark();
	ImGuiStyle& style{ ImGui::GetStyle() };
	style.WindowRounding = 0.0f;
	style.ChildRounding = 2.0f;
	style.FrameRounding = 2.0f;
	style.PopupRounding = 3.0f;
	style.ScrollbarRounding = 3.0f;
	style.GrabRounding = 2.0f;
	style.WindowPadding = ImVec2{ 7.0f, 7.0f };
	style.FramePadding = ImVec2{ 5.0f, 3.0f };
	style.ItemSpacing = ImVec2{ 5.0f, 4.0f };
	style.ItemInnerSpacing = ImVec2{ 4.0f, 3.0f };
	style.IndentSpacing = 14.0f;
}

void GlfwErrorCallback(int error, const char* description) {
	std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

} // namespace demo

int main() {
	glfwSetErrorCallback(demo::GlfwErrorCallback);
	if (!glfwInit()) {
		return 1;
	}

#if defined(__APPLE__)
	const char* glsl_version{ "#version 150" };
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	const char* glsl_version{ "#version 330" };
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

	GLFWwindow* window{ glfwCreateWindow(1650, 950, "Protegon Events + Script Sequences Demo", nullptr, nullptr) };
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	if (!gladLoadGL(glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	demo::ConfigureStyle();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ptgn::Manager manager;
	demo::GlobalScriptSequenceRegistry registry;
	auto entities{ demo::MakeDemoEntities(manager, registry) };
	demo::PrefabRegistry prefabs{ demo::MakeDemoPrefabs() };
	demo::DemoRuntimeContext runtime;
	runtime.entities = &entities;
	int selected_entity{ 0 };
	int selected_prefab{ -1 };
	bool inspect_prefab{ false };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		const float dt{ ImGui::GetIO().DeltaTime };

		demo::UpdatePlayerInput(window, entities, dt);
		demo::UpdateOverlapEvents(entities, runtime);
		demo::DispatchEvents(entities, registry, runtime);

		for (auto& entity : entities) {
			auto* scripts{ demo::TryGetScripts(entity) };
			if (!scripts) {
				continue;
			}

			for (auto& sequence : scripts->sequences) {
				demo::UpdateScriptSequence(entity, sequence, dt, registry, runtime);
			}
		}

		// Sequence items may emit more events. Dispatch them after sequence updates.
		demo::DispatchEvents(entities, registry, runtime);
		demo::UpdateActivity(runtime, dt);
		demo::DrawApplication(
			manager, entities, selected_entity, registry, runtime, prefabs, selected_prefab,
			inspect_prefab
		);

		ImGui::Render();
		int width{ 0 };
		int height{ 0 };
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.055f, 0.060f, 0.070f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
