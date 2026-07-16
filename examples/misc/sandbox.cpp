// behavior_component_inspector_demo.cpp
//
// Standalone Dear ImGui mock-up for a compact Behaviors entity component.
//
// Flat sequence entry types:
//   - Action
//   - Timed Action
//   - Wait
//   - Emit Signal
//
// Also demonstrates:
//   - Prefab creation and reflected component editing.
//   - Configurable single-prefab Spawn Entity action with random placement.
//   - Reflected multi-component Add/Remove Components actions.
//   - Separate start and stop trigger sections.
//   - Lifecycle callbacks and completion cleanup.
//
// Behaviors can be local to an entity component or references to shared global
// definitions. Runtime state remains per entity binding even for global behaviors.
//
// Expected dependencies: Dear ImGui, GLFW, GLAD/OpenGL, and the standard
// imgui_impl_glfw / imgui_impl_opengl3 backends.

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <imgui.h>

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
	Signal,
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
	Action,
	TimedAction,
	Wait,
	EmitSignal
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
	BehaviorEntity,
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

enum class ActionKind {
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
	Action,
	EmitSignal
};

constexpr std::array kTriggerNames{ "On Create",	  "Signal",			 "Key Pressed",
									"Key Released",	  "Key Held",		 "Mouse Pressed",
									"Mouse Released", "Mouse Held",		 "Overlap Start",
									"Overlap Stop",	  "Collision Start", "Collision Stop" };
constexpr std::array kReentryNames{ "Ignore", "Restart", "Queue", "Parallel" };
constexpr std::array kSequenceItemNames{
	"Action", "Timed Action", "Wait", "Emit Signal"
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
constexpr std::array kLifecycleCallbackKindNames{ "Action", "Emit Signal" };

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
			case SpawnOrigin::BehaviorEntity:
				return "Use the behavior entity as the placement origin. X and Y are offsets.";
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
	draw_item(TriggerKind::Signal);
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

void DrawCountControl(
	const char* label, int& value, bool disabled = false, const char* tooltip = nullptr
) {
	value = std::max(0, value);

	constexpr float button_width{ 22.0f };
	constexpr float text_button_spacing{ 3.0f };
	constexpr float button_spacing{ 3.0f };
	const std::string widest_text{ std::string{ label } + ": 000" };
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
	if (ImGui::Button("+", ImVec2{ button_width, 0.0f })) {
		++value;
	}

	ImGui::SameLine(0.0f, button_spacing);
	ImGui::BeginDisabled(value == 0);

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
	TextBuffer<80> signal{ "door.opened" };
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
struct MoveToParams { float destination[2]{ 0.0f, 64.0f }; bool relative{ true }; };
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
	SpawnOrigin origin{ SpawnOrigin::BehaviorEntity };
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

using ActionParameters = std::variant<
	SetVisibleParams, MoveToParams, RotateToParams, PlayAudioParams, SetColliderModeParams,
	ApplyDamageParams, SpawnEntityParams, AddComponentParams, RemoveComponentParams>;

struct ActionDefinition {
	ActionKind kind{ ActionKind::SetVisible };
	ActionParameters parameters{ SetVisibleParams{} };
};

struct ActionDescriptor {
	ActionKind kind;
	const char* key;
	const char* label;
	const char* group;
	const char* description;
	bool supports_timed;
};

constexpr std::array kActionRegistry{
	ActionDescriptor{ ActionKind::SpawnEntity, "engine.spawn_entity", "Spawn Entity", "Entity",
					  "Spawns one or more instances of one prefab with configurable placement.",
					  false },
	ActionDescriptor{
		ActionKind::AddComponent, "engine.add_component", "Add Components", "Entity",
		"Adds one or more registered components with reflected serialized values to the owner.",
		false },
	ActionDescriptor{ ActionKind::RemoveComponent, "engine.remove_component", "Remove Components",
					  "Entity",
					  "Removes one or more selected registered components from the owner.", false },
	ActionDescriptor{ ActionKind::SetVisible, "engine.set_visible", "Set Visible", "Entity",
					  "Changes entity visibility.", false },
	ActionDescriptor{ ActionKind::MoveTo, "engine.move_to", "Move To", "Transform",
					  "Moves an entity to a target position.", true },
	ActionDescriptor{ ActionKind::RotateTo, "engine.rotate_to", "Rotate To", "Transform",
					  "Rotates an entity to a target angle.", true },
	ActionDescriptor{ ActionKind::PlayAudio, "engine.play_audio", "Play Audio", "Audio",
					  "Plays an audio asset.", false },
	ActionDescriptor{ ActionKind::SetColliderMode, "engine.set_collider_mode", "Set Collider Mode",
					  "Physics", "Changes the collider mode.", false },
	ActionDescriptor{ ActionKind::ApplyDamage, "game.apply_damage", "Apply Damage", "Game",
					  "Example user-registered action.", false },
};

const ActionDescriptor& GetActionDescriptor(ActionKind kind) {
	const auto it{ std::ranges::find_if(kActionRegistry, [kind](const auto& descriptor) {
		return descriptor.kind == kind;
	}) };
	return it != kActionRegistry.end() ? *it : kActionRegistry.front();
}

ActionDefinition MakeAction(ActionKind kind) {
	ActionDefinition action;
	action.kind = kind;

	switch (kind) {
		case ActionKind::SetVisible: action.parameters = SetVisibleParams{}; break;
		case ActionKind::MoveTo: action.parameters = MoveToParams{}; break;
		case ActionKind::RotateTo: action.parameters = RotateToParams{}; break;
		case ActionKind::PlayAudio: action.parameters = PlayAudioParams{}; break;
		case ActionKind::SetColliderMode: action.parameters = SetColliderModeParams{}; break;
		case ActionKind::ApplyDamage: action.parameters = ApplyDamageParams{}; break;
		case ActionKind::SpawnEntity:	  action.parameters = SpawnEntityParams{}; break;
		case ActionKind::AddComponent: action.parameters = AddComponentParams{}; break;
		case ActionKind::RemoveComponent: action.parameters = RemoveComponentParams{}; break;
	}

	return action;
}

struct LifecycleCallbackDefinition {
	Id id{ NextId() };
	bool enabled{ true };
	LifecycleEventKind event{ LifecycleEventKind::Complete };
	LifecycleCallbackKind kind{ LifecycleCallbackKind::EmitSignal };
	ActionDefinition action{ MakeAction(ActionKind::SetVisible) };
	TextBuffer<80> signal{ "behavior.completed" };
};

struct ActionItem { ActionDefinition action; };
struct TimedActionItem {
	ActionDefinition action{ MakeAction(ActionKind::MoveTo) };
	float duration_ms{ 300.0f };
	Ease ease{ Ease::Linear };
	int additional_repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };
};

std::string TimedFlagsPreview(const TimedActionItem& timed) {
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

void DrawTimedFlagsCombo(TimedActionItem& timed) {
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
struct EmitSignalItem { TextBuffer<80> signal{ "door.opened" }; };

using SequenceItemData = std::variant<ActionItem, TimedActionItem, WaitItem, EmitSignalItem>;

struct SequenceItem {
	Id id{ NextId() };
	bool enabled{ true };
	SequenceItemKind kind{ SequenceItemKind::Action };
	SequenceItemData data{ ActionItem{} };
};

SequenceItem MakeSequenceItem(SequenceItemKind kind) {
	SequenceItem item;
	item.kind = kind;

	switch (kind) {
		case SequenceItemKind::Action: item.data = ActionItem{}; break;
		case SequenceItemKind::TimedAction: item.data = TimedActionItem{}; break;
		case SequenceItemKind::Wait: item.data = WaitItem{}; break;
		case SequenceItemKind::EmitSignal: item.data = EmitSignalItem{}; break;
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

struct BehaviorDefinition {
	Id id{ NextId() };
	TextBuffer<80> name{ "New Behavior" };
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
	int completed_runs{ 0 };
};

struct BehaviorBinding {
	Id id{ NextId() };
	bool enabled{ true };
	bool global_reference{ false };
	Id global_behavior_id{ 0 };
	BehaviorDefinition local_definition;
	RuntimeState runtime;
};

struct BehaviorsComponent { std::vector<BehaviorBinding> bindings; };

struct EntityData {
	Id id{ NextId() };
	TextBuffer<64> name{ "Entity" };
	TextBuffer<64> tag{};
	float position[2]{ 0.0f, 0.0f };
	float rotation{ 0.0f };
	float scale[2]{ 1.0f, 1.0f };
	bool visible{ true };
	std::optional<BehaviorsComponent> behaviors;
};

struct GlobalBehaviorRegistry {
	std::vector<BehaviorDefinition> definitions;

	BehaviorDefinition* Find(Id id) {
		const auto it{ std::ranges::find_if(definitions, [id](const auto& definition) {
			return definition.id == id;
		}) };
		return it != definitions.end() ? &*it : nullptr;
	}

	const BehaviorDefinition* Find(Id id) const {
		return const_cast<GlobalBehaviorRegistry*>(this)->Find(id);
	}

	BehaviorDefinition* FindByName(std::string_view name) {
		const auto it{ std::ranges::find_if(definitions, [name](const auto& definition) {
			return definition.name.View() == name;
		}) };
		return it != definitions.end() ? &*it : nullptr;
	}

	const BehaviorDefinition* FindByName(std::string_view name) const {
		return const_cast<GlobalBehaviorRegistry*>(this)->FindByName(name);
	}
};

BehaviorDefinition* ResolveBehavior(BehaviorBinding& binding, GlobalBehaviorRegistry& registry) {
	return binding.global_reference ? registry.Find(binding.global_behavior_id) : &binding.local_definition;
}

const BehaviorDefinition* ResolveBehavior(const BehaviorBinding& binding, const GlobalBehaviorRegistry& registry) {
	return binding.global_reference ? registry.Find(binding.global_behavior_id) : &binding.local_definition;
}

BehaviorDefinition CloneBehavior(const BehaviorDefinition& source) {
	BehaviorDefinition copy{ source };
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

struct SignalEvent {
	std::string name;
	Id source_entity{ 0 };
	Id source_behavior{ 0 };
};

struct ActivityEntry {
	std::string text;
};

struct DemoRuntimeContext {
	std::deque<SignalEvent> pending_signals;
	std::vector<ActivityEntry> activity;
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

void ExecuteAction(
	const ActionDefinition& action,
	const EntityData& entity,
	const BehaviorDefinition& behavior,
	DemoRuntimeContext& context,
	bool timed
) {
	std::string detail{ GetActionDescriptor(action.kind).label };

	switch (action.kind) {
		case ActionKind::SpawnEntity: {
			const auto& params{ std::get<SpawnEntityParams>(action.parameters) };
			detail += " [" + std::string{ params.prefab_key.Data() } +
					  ", count=" + std::to_string(std::max(1, params.count)) +
					  ", origin=" + kSpawnOriginNames[static_cast<std::size_t>(params.origin)] +
					  ", area=" + kSpawnAreaNames[static_cast<std::size_t>(params.area)] + "]";
			break;
		}

		case ActionKind::AddComponent:
			detail += " [";
			for (std::size_t i{ 0 };
				 i < std::get<AddComponentParams>(action.parameters).components.size(); ++i) {
				if (i != 0) {
					detail += ", ";
				}
				detail += GetComponentDescriptor(
							  std::get<AddComponentParams>(action.parameters).components[i].kind
				)
							  .label;
			}
			detail += "]";
			break;

		case ActionKind::RemoveComponent:
			detail += " [" +
					  ComponentSelectionPreview(
						  std::get<RemoveComponentParams>(action.parameters).components, "None"
					  ) +
					  "]";
			break;

		case ActionKind::SetVisible:
		case ActionKind::MoveTo:
		case ActionKind::RotateTo:
		case ActionKind::PlayAudio:
		case ActionKind::SetColliderMode:
		case ActionKind::ApplyDamage:
			break;
	}

	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + behavior.name.Data() +
			(timed ? " timed: " : " action: ") + detail
	);
}

void EmitSignal(
	const EmitSignalItem& emit,
	const EntityData& entity,
	const BehaviorDefinition& behavior,
	DemoRuntimeContext& context
) {
	if (emit.signal.Empty()) {
		return;
	}
	context.pending_signals.push_back({ std::string{ emit.signal.Data() }, entity.id, behavior.id });
	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + behavior.name.Data() +
			" emitted \"" + emit.signal.Data() + "\""
	);
}

void InvokeLifecycleCallbacks(
	LifecycleEventKind event, EntityData& entity, const BehaviorDefinition& behavior,
	DemoRuntimeContext& context
) {
	for (const auto& callback : behavior.lifecycle_callbacks) {
		if (!callback.enabled || callback.event != event) {
			continue;
		}

		switch (callback.kind) {
			case LifecycleCallbackKind::Action:
				ExecuteAction(callback.action, entity, behavior, context, false);
				break;

			case LifecycleCallbackKind::EmitSignal: {
				EmitSignalItem emit;
				emit.signal = callback.signal;
				EmitSignal(emit, entity, behavior, context);
				break;
			}
		}
	}
}

bool IsTimedItem(const SequenceItem& item) {
	return item.kind == SequenceItemKind::Wait || item.kind == SequenceItemKind::TimedAction;
}

float GetItemDuration(const SequenceItem& item) {
	switch (item.kind) {
		case SequenceItemKind::Wait:
			return std::max(0.0f, std::get<WaitItem>(item.data).duration_ms);
		case SequenceItemKind::TimedAction:
			return std::max(0.0f, std::get<TimedActionItem>(item.data).duration_ms);
		case SequenceItemKind::Action:
		case SequenceItemKind::EmitSignal:
			return 0.0f;
	}
	return 0.0f;
}

void StartBehaviorRuntime(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context,
	bool force_restart = true
);

void FinishBehavior(
	EntityData& entity,
	BehaviorBinding& binding,
	const BehaviorDefinition& behavior,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };
	const bool queued{ runtime.queued };

	runtime.running = false;
	runtime.paused = false;
	runtime.completed = true;
	runtime.queued	  = false;
	runtime.started_item_index.reset();
	++runtime.completed_runs;

	AddActivity(
		context, std::string{ entity.name.Data() } + " / " + behavior.name.Data() + " completed"
	);
	InvokeLifecycleCallbacks(LifecycleEventKind::Complete, entity, behavior, context);

	if (behavior.destroy_on_complete) {
		const int completed_runs{ runtime.completed_runs };
		binding.runtime				   = {};
		binding.runtime.completed_runs = completed_runs;
		AddActivity(
			context, std::string{ entity.name.Data() } + " / " + behavior.name.Data() +
						 " destroyed its runtime on complete"
		);
		return;
	}

	if (queued) {
		StartBehaviorRuntime(entity, binding, registry, context);
	}
}

void ProcessImmediateItems(
	EntityData& entity,
	BehaviorBinding& binding,
	const BehaviorDefinition& behavior,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };

	while (runtime.running && runtime.item_index < behavior.sequence.size()) {
		const auto& item{ behavior.sequence[runtime.item_index] };

		if (!item.enabled) {
			++runtime.item_index;
			runtime.started_item_index.reset();
			continue;
		}

		if (runtime.started_item_index != runtime.item_index) {
			runtime.started_item_index = runtime.item_index;
			InvokeLifecycleCallbacks(LifecycleEventKind::PointStart, entity, behavior, context);
		}

		if (IsTimedItem(item)) {
			if (GetItemDuration(item) <= 0.0f) {
				if (item.kind == SequenceItemKind::TimedAction) {
					ExecuteAction(
						std::get<TimedActionItem>(item.data).action, entity, behavior, context, true
					);
				}

				InvokeLifecycleCallbacks(
					LifecycleEventKind::PointComplete, entity, behavior, context
				);
				++runtime.item_index;
				runtime.started_item_index.reset();
				runtime.elapsed_ms = 0.0f;
				continue;
			}

			break;
		}

		switch (item.kind) {
			case SequenceItemKind::Action:
				ExecuteAction(
					std::get<ActionItem>(item.data).action, entity, behavior, context, false
				);
				break;

			case SequenceItemKind::EmitSignal:
				EmitSignal(std::get<EmitSignalItem>(item.data), entity, behavior, context);
				break;

			case SequenceItemKind::TimedAction:
			case SequenceItemKind::Wait:
				break;
		}

		InvokeLifecycleCallbacks(LifecycleEventKind::PointComplete, entity, behavior, context);
		++runtime.item_index;
		runtime.started_item_index.reset();
		runtime.elapsed_ms = 0.0f;
	}

	if (runtime.running && runtime.item_index >= behavior.sequence.size()) {
		FinishBehavior(entity, binding, behavior, registry, context);
	}
}

void StartBehaviorRuntime(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context,
	bool force_restart
) {
	if (!binding.enabled) {
		return;
	}

	const auto* behavior{ ResolveBehavior(binding, registry) };
	if (!behavior) {
		return;
	}

	auto& runtime{ binding.runtime };

	if (runtime.running && !force_restart) {
		switch (behavior->reentry) {
			case ReentryMode::IgnoreWhileRunning: return;

			case ReentryMode::Restart:			  break;

			case ReentryMode::Queue:			  runtime.queued = true; return;

			case ReentryMode::Parallel:			  break;
		}
	}

	const bool resetting{ force_restart &&
						  (runtime.running || runtime.paused || runtime.completed ||
						   runtime.started_item_index.has_value()) };

	if (resetting) {
		InvokeLifecycleCallbacks(LifecycleEventKind::Reset, entity, *behavior, context);
	}

	runtime.running = !behavior->sequence.empty();
	runtime.paused = false;
	runtime.completed  = false;
	runtime.queued	   = false;
	runtime.item_index = 0;
	runtime.started_item_index.reset();
	runtime.elapsed_ms = 0.0f;

	AddActivity(
		context, std::string{ entity.name.Data() } + " / " + behavior->name.Data() + " started"
	);
	InvokeLifecycleCallbacks(LifecycleEventKind::Start, entity, *behavior, context);

	if (runtime.running) {
		ProcessImmediateItems(entity, binding, *behavior, registry, context);
	} else {
		FinishBehavior(entity, binding, *behavior, registry, context);
	}
}

void SetBehaviorPaused(
	EntityData& entity, BehaviorBinding& binding, const BehaviorDefinition& behavior,
	DemoRuntimeContext& context, bool paused
) {
	if (!binding.runtime.running || binding.runtime.paused == paused) {
		return;
	}

	binding.runtime.paused = paused;
	InvokeLifecycleCallbacks(
		paused ? LifecycleEventKind::Pause : LifecycleEventKind::Resume, entity, behavior, context
	);
	AddActivity(
		context, std::string{ entity.name.Data() } + " / " + behavior.name.Data() +
					 (paused ? " paused" : " resumed")
	);
}

void StopBehaviorRuntime(
	EntityData& entity, BehaviorBinding& binding, const BehaviorDefinition& behavior,
	DemoRuntimeContext& context
) {
	if (!binding.runtime.running && !binding.runtime.paused) {
		binding.runtime = {};
		return;
	}

	InvokeLifecycleCallbacks(LifecycleEventKind::Stop, entity, behavior, context);
	AddActivity(
		context, std::string{ entity.name.Data() } + " / " + behavior.name.Data() + " stopped"
	);
	binding.runtime = {};
}

void UpdateBehaviorRuntime(
	EntityData& entity,
	BehaviorBinding& binding,
	float delta_seconds,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };
	if (!runtime.running || runtime.paused) {
		return;
	}

	const auto* behavior{ ResolveBehavior(binding, registry) };
	if (!behavior || runtime.item_index >= behavior->sequence.size()) {
		if (behavior) {
			FinishBehavior(entity, binding, *behavior, registry, context);
		}
		return;
	}

	const auto& item{ behavior->sequence[runtime.item_index] };
	if (!item.enabled || !IsTimedItem(item)) {
		ProcessImmediateItems(entity, binding, *behavior, registry, context);
		return;
	}

	if (runtime.started_item_index != runtime.item_index) {
		runtime.started_item_index = runtime.item_index;
		InvokeLifecycleCallbacks(LifecycleEventKind::PointStart, entity, *behavior, context);
	}

	runtime.elapsed_ms += delta_seconds * 1000.0f;
	// InvokeLifecycleCallbacks(LifecycleEventKind::Progress, entity, *behavior, context);

	if (runtime.elapsed_ms < GetItemDuration(item)) {
		return;
	}

	if (item.kind == SequenceItemKind::TimedAction) {
		ExecuteAction(
			std::get<TimedActionItem>(item.data).action, entity, *behavior, context, true
		);
	}

	InvokeLifecycleCallbacks(LifecycleEventKind::PointComplete, entity, *behavior, context);
	++runtime.item_index;
	runtime.started_item_index.reset();
	runtime.elapsed_ms = 0.0f;
	ProcessImmediateItems(entity, binding, *behavior, registry, context);
}

bool MatchesSignalTrigger(const std::vector<TriggerDefinition>& triggers, std::string_view signal) {
	return std::ranges::any_of(triggers, [signal](const auto& trigger) {
		return trigger.enabled && trigger.kind == TriggerKind::Signal &&
			   trigger.signal.View() == signal;
	});
}

void DispatchSignals(
	std::vector<EntityData>& entities,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	while (!context.pending_signals.empty()) {
		SignalEvent event{ std::move(context.pending_signals.front()) };
		context.pending_signals.pop_front();

		for (auto& entity : entities) {
			if (!entity.behaviors) {
				continue;
			}
			for (auto& binding : entity.behaviors->bindings) {
				const auto* behavior{ ResolveBehavior(binding, registry) };
				if (!behavior) {
					continue;
				}

				// Stop triggers take precedence when the same signal appears in both lists.
				if (MatchesSignalTrigger(behavior->stop_triggers, event.name)) {
					AddActivity(
						context, std::string{ entity.name.Data() } + " / " + behavior->name.Data() +
									 " received stop signal \"" + event.name + "\""
					);
					StopBehaviorRuntime(entity, binding, *behavior, context);
					continue;
				}

				if (!MatchesSignalTrigger(behavior->triggers, event.name)) {
					continue;
				}

				AddActivity(
					context, std::string{ entity.name.Data() } + " / " + behavior->name.Data() +
								 " received start signal \"" + event.name + "\""
				);
				StartBehaviorRuntime(entity, binding, registry, context, false);
			}
		}
	}
}

void UpdateActivity(DemoRuntimeContext&, float) {}

float GetRuntimeProgress(const BehaviorBinding& binding, const BehaviorDefinition& behavior) {
	const auto& runtime{ binding.runtime };
	if (runtime.completed) {
		return 1.0f;
	}
	if (!runtime.running || runtime.item_index >= behavior.sequence.size()) {
		return 0.0f;
	}
	const float duration{ GetItemDuration(behavior.sequence[runtime.item_index]) };
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

		case TriggerKind::Signal:	   return std::string{ "Signal: " } + trigger.signal.Data();

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
		case SequenceItemKind::Action:
			return GetActionDescriptor(std::get<ActionItem>(item.data).action.kind).label;
		case SequenceItemKind::TimedAction: {
			const auto& timed{ std::get<TimedActionItem>(item.data) };
			std::snprintf(buffer, sizeof(buffer), "%s  %.0fms", GetActionDescriptor(timed.action.kind).label, timed.duration_ms);
			return buffer;
		}
		case SequenceItemKind::Wait:
			std::snprintf(buffer, sizeof(buffer), "%.0fms", std::get<WaitItem>(item.data).duration_ms);
			return buffer;
		case SequenceItemKind::EmitSignal:
			return std::get<EmitSignalItem>(item.data).signal.Data();
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

void DrawAddComponentButton(ActionDefinition& action) {
	auto* params{ std::get_if<AddComponentParams>(&action.parameters) };
	if (action.kind != ActionKind::AddComponent || !params) {
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
		DrawItemTooltip("Make the behavior owner the spawned entity's parent.");

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
						  : "Apply the behavior owner's rotation to each spawned entity."
			);
		}

		ImGui::Checkbox("Parent Scale", &spawn.inherit_owner_scale);
		DrawItemTooltip("Apply the behavior owner's scale to each spawned entity.");

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

void DrawActionParametersCompact(
	ActionDefinition& action, float left_screen_x, const PrefabRegistry& prefabs
) {
	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	const float available_width{ std::max(1.0f, right_screen_x - left_screen_x) };

	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });

	switch (action.kind) {
		case ActionKind::SetVisible: {
			auto& p{ std::get<SetVisibleParams>(action.parameters) };
			ImGui::Checkbox("Visible", &p.visible);
			break;
		}

		case ActionKind::MoveTo: {
			auto& p{ std::get<MoveToParams>(action.parameters) };

			if (ImGui::BeginTable(
					"MoveToParams", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 53.0f);
				ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Relative", ImGuiTableColumnFlags_WidthFixed, 88.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("Position");

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat2(
					"##Destination", p.destination, 1.0f, -100000.0f, 100000.0f, "%.0f"
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::Checkbox("Relative", &p.relative);
				ImGui::EndTable();
			}
			break;
		}

		case ActionKind::RotateTo: {
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

		case ActionKind::PlayAudio: {
			auto& p{ std::get<PlayAudioParams>(action.parameters) };

			if (ImGui::BeginTable(
					"AudioParams", 3, ImGuiTableFlags_SizingStretchProp,
					ImVec2{ available_width, 0.0f }
				)) {
				ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Loops", ImGuiTableColumnFlags_WidthFixed, 126.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Audio", p.asset.Data(), p.asset.Size());

				ImGui::TableSetColumnIndex(1);
				DrawVolumeControl(p.volume);

				ImGui::TableSetColumnIndex(2);
				DrawCountControl("Loops", p.loops);

				ImGui::EndTable();
			}
			break;
		}

		case ActionKind::SetColliderMode: {
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

		case ActionKind::ApplyDamage: {
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

		case ActionKind::SpawnEntity: {
			auto& spawn{ std::get<SpawnEntityParams>(action.parameters) };
			spawn.count				= std::max(1, spawn.count);
			spawn.rectangle_size[0] = std::max(0.0f, spawn.rectangle_size[0]);
			spawn.rectangle_size[1] = std::max(0.0f, spawn.rectangle_size[1]);
			spawn.radius			= std::max(0.0f, spawn.radius);

			const float count_button_width{ ImGui::GetFrameHeight() };
			const float count_text_button_spacing{ 3.0f };
			const float count_button_spacing{ 3.0f };
			const float count_text_width{ ImGui::CalcTextSize("Count: 000").x };
			const float count_control_width{ count_text_width + count_text_button_spacing +
											 count_button_width * 2.0f + count_button_spacing };

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
				const float count_text_start_x{ ImGui::GetCursorScreenPos().x };
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Count: %d", spawn.count);
				DrawItemTooltip("Number of prefab instances created by this action.");

				ImGui::PushID("SpawnCount");
				ImGui::SameLine();
				ImGui::SetCursorScreenPos(
					ImVec2{ count_text_start_x + count_text_width + count_text_button_spacing,
							ImGui::GetCursorScreenPos().y }
				);
				if (ImGui::Button("+", ImVec2{ count_button_width, ImGui::GetFrameHeight() })) {
					++spawn.count;
				}

				ImGui::SameLine(0.0f, count_button_spacing);
				ImGui::BeginDisabled(spawn.count <= 1);
				if (ImGui::Button("-", ImVec2{ count_button_width, ImGui::GetFrameHeight() })) {
					spawn.count = std::max(1, spawn.count - 1);
				}
				ImGui::EndDisabled();

				ImGui::PopID();
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
					spawn.origin == SpawnOrigin::BehaviorEntity
						? "Horizontal offset from the behavior entity."
						: "World-space X position."
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##SpawnY", &spawn.center[1], 1.0f, -100000.0f, 100000.0f, "Y: %.0f"
				);
				DrawItemTooltip(
					spawn.origin == SpawnOrigin::BehaviorEntity
						? "Vertical offset from the behavior entity."
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

		case ActionKind::AddComponent: {
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

		case ActionKind::RemoveComponent: {
			auto& p{ std::get<RemoveComponentParams>(action.parameters) };
			ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
			DrawComponentMultiSelectCombo(
				"##RemoveComponents", p.components, "Select components to remove", available_width
			);
			break;
		}
	}
}

void DrawActionParameters(ActionDefinition& action, const PrefabRegistry& prefabs) {
	DrawActionParametersCompact(action, ImGui::GetCursorScreenPos().x, prefabs);
}

bool DrawActionPicker(
	const char* label,
	ActionDefinition& action,
	bool timed_only,
	float width = -FLT_MIN
) {
	bool changed{ false };
	ImGui::SetNextItemWidth(width);

	if (ImGui::BeginCombo(label, GetActionDescriptor(action.kind).label)) {
		auto draw_action = [&](ActionKind kind) {
			const auto& descriptor{ GetActionDescriptor(kind) };
			if (timed_only && !descriptor.supports_timed) {
				return;
			}

			const bool selected{ action.kind == kind };
			if (ImGui::MenuItem(descriptor.label, nullptr, selected)) {
				action = MakeAction(kind);
				changed = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s\n%s", descriptor.description, descriptor.key);
			}
		};

		if (!timed_only && ImGui::BeginMenu("Entity")) {
			draw_action(ActionKind::SpawnEntity);
			ImGui::Separator();
			draw_action(ActionKind::AddComponent);
			draw_action(ActionKind::RemoveComponent);
			ImGui::Separator();
			draw_action(ActionKind::SetVisible);
			ImGui::EndMenu();
		}

		constexpr std::array groups{ "Transform", "Audio", "Physics", "Game" };
		for (const char* group : groups) {
			const bool has_entries{ std::ranges::any_of(
				kActionRegistry, [group, timed_only](const auto& descriptor) {
					return std::strcmp(descriptor.group, group) == 0 &&
						   (!timed_only || descriptor.supports_timed);
				}
			) };

			if (!has_entries || !ImGui::BeginMenu(group)) {
				continue;
			}

			for (const auto& descriptor : kActionRegistry) {
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

bool SupportsInlineAddButton(ActionKind kind) {
	return kind == ActionKind::AddComponent;
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
			case LifecycleCallbackKind::Action: {
				const bool show_add_button{ SupportsInlineAddButton(callback.action.kind) };
				const float add_width{ ImGui::GetFrameHeight() };
				const float spacing{ ImGui::GetStyle().ItemSpacing.x };
				const float picker_width{
					show_add_button
						? std::max(1.0f, ImGui::GetContentRegionAvail().x - add_width - spacing)
						: -FLT_MIN
				};

				DrawActionPicker("##CallbackAction", callback.action, false, picker_width);

				if (show_add_button && callback.action.kind == ActionKind::AddComponent) {
					ImGui::SameLine(0.0f, spacing);
					DrawAddComponentButton(callback.action);
				}
				break;
			}
			case LifecycleCallbackKind::EmitSignal:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputTextWithHint(
					"##CallbackSignal", "Signal name", callback.signal.Data(),
					callback.signal.Size()
				);
				DrawItemTooltip("Unique signal emitted when this lifecycle event occurs.");
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

	if (callback.kind == LifecycleCallbackKind::Action) {
		DrawActionParametersCompact(callback.action, callback_left_screen_x, prefabs);
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

void DrawLifecycleSection(BehaviorDefinition& behavior, const PrefabRegistry& prefabs) {
	char lifecycle_label[96]{};
	std::snprintf(
		lifecycle_label, sizeof(lifecycle_label), "Lifecycle (%zu)%s",
		behavior.lifecycle_callbacks.size(),
		behavior.destroy_on_complete ? "  [Destroy on Complete]" : ""
	);

	bool add_callback{ false };
	const bool lifecycle_open{ DrawAddableSectionHeader(
		"LifecycleSection", lifecycle_label, false, behavior.lifecycle_callbacks.empty(),
		"Optional lifecycle callbacks and completion cleanup.", "No lifecycle callbacks.",
		"Add a lifecycle callback.", add_callback
	) };

	if (add_callback) {
		behavior.lifecycle_callbacks.emplace_back();
	}

	if (!lifecycle_open) {
		return;
	}

	int remove_callback{ -1 };

	for (int i{ 0 }; i < static_cast<int>(behavior.lifecycle_callbacks.size()); ++i) {
		if (DrawLifecycleCallbackCompact(
				behavior.lifecycle_callbacks[static_cast<std::size_t>(i)], prefabs
			)) {
			remove_callback = i;
		}
	}

	if (remove_callback >= 0) {
		behavior.lifecycle_callbacks.erase(behavior.lifecycle_callbacks.begin() + remove_callback);
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
					stop_trigger ? "Delay before the behavior stops after the entity is created."
								 : "Delay before the behavior starts after the entity is created."
				);
				break;
			}

			case TriggerKind::Signal:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Signal", trigger.signal.Data(), trigger.signal.Size());
				DrawItemTooltip("Unique name of the signal to listen for.");
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
	trigger.kind		= stop_triggers ? TriggerKind::Signal : TriggerKind::OnCreate;
	trigger.duration_ms = 0.0f;
	if (stop_triggers) {
		trigger.signal.Assign("behavior.stop");
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
		stop_triggers ? "Triggers that stop this behavior." : "Triggers that start this behavior.",
		stop_triggers ? "No stop triggers: this behavior only stops manually or on completion."
					  : "No start triggers: this behavior is started manually.",
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
	const float duration_width{ 82.0f };
	const float repeats_width{ ImGui::CalcTextSize("Repeats: 000").x + 44.0f + 6.0f };
	const float add_width{ ImGui::GetFrameHeight() };
	const float remove_width{ ImGui::GetFrameHeight() };

	const SequenceItemKind displayed_kind{ item.kind };
	bool show_add_button{ false };

	if (displayed_kind == SequenceItemKind::Action) {
		show_add_button = SupportsInlineAddButton(std::get<ActionItem>(item.data).action.kind);
	}

	int column_count{ 4 };
	if (displayed_kind == SequenceItemKind::Action && show_add_button) {
		++column_count;
	} else if (displayed_kind == SequenceItemKind::TimedAction) {
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
			case SequenceItemKind::Action:
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
				if (show_add_button) {
					ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, add_width);
				}
				break;

			case SequenceItemKind::TimedAction:
				ImGui::TableSetupColumn(
					"Duration", ImGuiTableColumnFlags_WidthFixed, duration_width
				);
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Repeats", ImGuiTableColumnFlags_WidthFixed, repeats_width);
				break;

			case SequenceItemKind::Wait:
			case SequenceItemKind::EmitSignal:
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
			case SequenceItemKind::Action: {
				auto& action_item{ std::get<ActionItem>(item.data) };

				ImGui::TableSetColumnIndex(2);
				DrawActionPicker("##Action", action_item.action, false);

				if (show_add_button) {
					ImGui::TableSetColumnIndex(3);
					if (action_item.action.kind == ActionKind::AddComponent) {
						DrawAddComponentButton(action_item.action);
					}
				}
				break;
			}

			case SequenceItemKind::TimedAction: {
				auto& timed{ std::get<TimedActionItem>(item.data) };

				ImGui::TableSetColumnIndex(2);
				DrawDurationInput(
					"##Duration", timed.duration_ms, -FLT_MIN,
					"Duration of each timed-action cycle."
				);

				ImGui::TableSetColumnIndex(3);
				DrawActionPicker("##Action", timed.action, true);

				ImGui::TableSetColumnIndex(4);
				DrawCountControl(
					"Repeats", timed.additional_repeats, timed.infinite_repeats,
					"Additional full-duration cycles. Each repeat runs the timed action for the "
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

			case SequenceItemKind::EmitSignal: {
				ImGui::TableSetColumnIndex(2);
				auto& emit{ std::get<EmitSignalItem>(item.data) };
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Signal", emit.signal.Data(), emit.signal.Size());
				DrawItemTooltip("Unique name used to identify this signal.");
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

	if (item.kind == SequenceItemKind::TimedAction) {
		auto& timed{ std::get<TimedActionItem>(item.data) };
		const float right_screen_x{ ImGui::GetWindowPos().x +
									ImGui::GetWindowContentRegionMax().x };
		const float available_width{ std::max(1.0f, right_screen_x - type_left_screen_x) };

		ImGui::SetCursorScreenPos(ImVec2{ type_left_screen_x, ImGui::GetCursorScreenPos().y });
		if (ImGui::BeginTable(
				"TimedActionOptionsRow", 2, ImGuiTableFlags_SizingStretchSame,
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

	if (item.kind == SequenceItemKind::TimedAction) {
		DrawActionParametersCompact(
			std::get<TimedActionItem>(item.data).action, type_left_screen_x, prefabs
		);
	} else if (item.kind == SequenceItemKind::Action) {
		DrawActionParametersCompact(
			std::get<ActionItem>(item.data).action, type_left_screen_x, prefabs
		);
	}

	ImGui::PopID();
	return remove;
}

void DrawBehaviorSequence(
	BehaviorDefinition& behavior,
	BehaviorBinding& binding,
	const PrefabRegistry& prefabs
) {
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int i{ 0 }; i < static_cast<int>(behavior.sequence.size()); ++i) {
		auto& item{ behavior.sequence[static_cast<std::size_t>(i)] };
		const bool active{
			binding.runtime.running &&
			binding.runtime.item_index == static_cast<std::size_t>(i)
		};
		bool duplicate{ false };

		if (DrawSequenceItemCompact(
				item,
				i,
				active,
				active ? GetRuntimeProgress(binding, behavior) : 0.0f,
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
		MoveItem(behavior.sequence, move_from, move_to);
	}

	if (duplicate_index >= 0) {
		auto copy{ behavior.sequence[static_cast<std::size_t>(duplicate_index)] };
		copy.id = NextId();
		behavior.sequence.insert(
			behavior.sequence.begin() + duplicate_index + 1,
			std::move(copy)
		);
	}

	if (remove_index >= 0) {
		behavior.sequence.erase(behavior.sequence.begin() + remove_index);
		binding.runtime = {};
	}

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float button_width{
		(ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f
	};

	if (ImGui::Button("+ Action", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::Action));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Timed Action", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::TimedAction));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Wait", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::Wait));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Emit Signal", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::EmitSignal));
	}
}

void PromoteBindingToGlobal(BehaviorBinding& binding, GlobalBehaviorRegistry& registry) {
	if (binding.global_reference) {
		return;
	}

	if (auto* existing{ registry.FindByName(binding.local_definition.name.View()) }) {
		binding.global_reference   = true;
		binding.global_behavior_id = existing->id;
		binding.local_definition   = {};
		binding.runtime			   = {};
		return;
	}

	BehaviorDefinition global{ std::move(binding.local_definition) };
	const Id id{ global.id };
	registry.definitions.push_back(std::move(global));
	binding.global_reference = true;
	binding.global_behavior_id = id;
	binding.local_definition = {};
	binding.runtime = {};
}

void DetachBindingToLocal(BehaviorBinding& binding, GlobalBehaviorRegistry& registry) {
	if (!binding.global_reference) {
		return;
	}
	const auto* global{ registry.Find(binding.global_behavior_id) };
	binding.local_definition   = global ? CloneBehavior(*global) : BehaviorDefinition{};
	binding.global_reference = false;
	binding.global_behavior_id = 0;
	binding.runtime = {};
}

void DrawRuntimeButtons(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	if (ImGui::BeginTable("RuntimeButtons", 3, ImGuiTableFlags_SizingStretchSame)) {
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		if (ImGui::Button(
				binding.runtime.running ? "Restart" : "Start",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			StartBehaviorRuntime(entity, binding, registry, context);
		}

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginDisabled(!binding.runtime.running);
		if (ImGui::Button(
				binding.runtime.paused ? "Resume" : "Pause",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			if (const auto* behavior{ ResolveBehavior(binding, registry) }) {
				SetBehaviorPaused(entity, binding, *behavior, context, !binding.runtime.paused);
			}
		}
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(2);
		if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
			if (const auto* behavior{ ResolveBehavior(binding, registry) }) {
				StopBehaviorRuntime(entity, binding, *behavior, context);
			} else {
				binding.runtime = {};
			}
		}

		ImGui::EndTable();
	}
}

bool DrawBehaviorBinding(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context,
	const PrefabRegistry& prefabs
) {
	auto* behavior{ ResolveBehavior(binding, registry) };

	if (!behavior) {
		ImGui::TextDisabled("Missing global behavior");
		return false;
	}

	bool remove{ false };
	ImGui::PushID(static_cast<int>(binding.id));

	char header[192]{};
	std::snprintf(
		header, sizeof(header), "%s%s%s%s", binding.runtime.running ? "> " : "",
		binding.enabled ? "" : "[Disabled] ", binding.global_reference ? "[Global] " : "",
		behavior->name.Data()
	);

	const bool open{ ImGui::TreeNodeEx(
		"##Behavior",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
		"%s", header
	) };

	if (ImGui::BeginPopupContextItem("BehaviorContextMenu")) {
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
		if (ImGui::BeginTable("BehaviorMainRow", 6, ImGuiTableFlags_SizingStretchProp)) {
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
			ImGui::InputText("##Name", behavior->name.Data(), behavior->name.Size());

			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button(
					binding.runtime.running ? "Restart" : "Start",
					ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
				)) {
				StartBehaviorRuntime(entity, binding, registry, context);
			}

			ImGui::TableSetColumnIndex(3);
			ImGui::BeginDisabled(!binding.runtime.running);
			if (ImGui::Button(
					binding.runtime.paused ? "Resume" : "Pause",
					ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
				)) {
				SetBehaviorPaused(entity, binding, *behavior, context, !binding.runtime.paused);
			}
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(4);
			if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
				StopBehaviorRuntime(entity, binding, *behavior, context);
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

		DrawEnumCombo("##Reentry", behavior->reentry, kReentryNames, reentry_width);
		DrawItemTooltip(
			"Controls what happens if this behavior is triggered while already running."
		);

		ImGui::SameLine(0.0f, reentry_checkbox_spacing);
		bool global{ binding.global_reference };
		if (ImGui::Checkbox("Global", &global)) {
			if (global) {
				PromoteBindingToGlobal(binding, registry);
			} else {
				DetachBindingToLocal(binding, registry);
			}
			behavior = ResolveBehavior(binding, registry);
		}
		DrawItemTooltip(
			binding.global_reference ? "Shared behavior definition used by multiple entities."
									 : "Local behavior definition owned by this entity."
		);

		ImGui::SameLine();
		ImGui::Checkbox("Destroy on Complete", &behavior->destroy_on_complete);
		DrawItemTooltip(
			"Destroys the transient behavior runtime after completion. The owning entity and "
			"behavior definition remain."
		);

		DrawLifecycleSection(*behavior, prefabs);

		DrawTriggerSection(behavior->triggers, false);
		DrawTriggerSection(behavior->stop_triggers, true);

		char sequence_label[64]{};
		std::snprintf(sequence_label, sizeof(sequence_label), "Sequence (%zu)", behavior->sequence.size());

		const bool sequence_open{ ImGui::TreeNodeEx(
			"##Sequence",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Framed,
			"%s", sequence_label
		) };

		if (sequence_open) {
			DrawBehaviorSequence(*behavior, binding, prefabs);
		}
	}

	ImGui::PopID();
	return remove;
}

BehaviorBinding MakeLocalBinding() {
	BehaviorBinding binding;
	TriggerDefinition on_create;
	on_create.kind		  = TriggerKind::OnCreate;
	on_create.duration_ms = 0.0f;
	binding.local_definition.triggers.push_back(std::move(on_create));
	return binding;
}

BehaviorBinding MakeGlobalBinding(Id id) {
	BehaviorBinding binding;
	binding.global_reference = true;
	binding.global_behavior_id = id;
	return binding;
}

void DrawAddBehaviorPopup(
	BehaviorsComponent& component,
	GlobalBehaviorRegistry& registry
) {
	if (!ImGui::BeginPopup("AddBehaviorPopup")) {
		return;
	}

	if (ImGui::MenuItem("Create New Behavior")) {
		component.bindings.push_back(MakeLocalBinding());
	}

	if (ImGui::BeginMenu("Existing Behavior")) {
		if (registry.definitions.empty()) {
			ImGui::TextDisabled("No global behaviors");
		}

		for (const auto& definition : registry.definitions) {
			const bool attached{ std::ranges::any_of(
				component.bindings, [&definition](const auto& binding) {
					return binding.global_reference && binding.global_behavior_id == definition.id;
				}
			) };

			ImGui::BeginDisabled(attached);

			if (ImGui::MenuItem(definition.name.Data())) {
				component.bindings.push_back(MakeGlobalBinding(definition.id));
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

void DrawBehaviorsComponent(
	EntityData& entity,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context,
	const PrefabRegistry& prefabs
) {
	auto& component{ *entity.behaviors };
	ImGui::PushID("BehaviorsComponent");

	char header[96]{};
	std::snprintf(header, sizeof(header), "Behaviors (%zu)", component.bindings.size());

	const bool open{ ImGui::TreeNodeEx(
		"##BehaviorsComponent",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"%s", header
	) };

	bool remove_component{ false };
	if (ImGui::BeginPopupContextItem("BehaviorsComponentContextMenu")) {
		if (ImGui::MenuItem("Delete Component")) {
			remove_component = true;
		}
		ImGui::EndPopup();
	}

	if (remove_component) {
		if (open) {
			ImGui::TreePop();
		}
		entity.behaviors.reset();
		ImGui::PopID();
		return;
	}

	if (open) {
		int remove_index{ -1 };
		for (int i{ 0 }; i < static_cast<int>(component.bindings.size()); ++i) {
			if (DrawBehaviorBinding(
					entity,
					component.bindings[static_cast<std::size_t>(i)],
					registry,
					context,
					prefabs
				)) {
				remove_index = i;
			}
		}

		if (remove_index >= 0) {
			component.bindings.erase(component.bindings.begin() + remove_index);
		}

		if (ImGui::Button("+ Add Behavior", ImVec2{ -FLT_MIN, 0.0f })) {
			ImGui::OpenPopup("AddBehaviorPopup");
		}
		DrawAddBehaviorPopup(component, registry);

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
	EntityData& entity, GlobalBehaviorRegistry& registry, DemoRuntimeContext& context,
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
	if (entity.behaviors) {
		DrawBehaviorsComponent(entity, registry, context, prefabs);
	}

	if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}
	if (ImGui::BeginPopup("AddComponentPopup")) {
		ImGui::BeginDisabled(entity.behaviors.has_value());
		if (ImGui::MenuItem("Behaviors")) {
			entity.behaviors.emplace();
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

std::vector<EntityData> MakeDemoEntities(GlobalBehaviorRegistry& registry) {
	BehaviorDefinition celebration;
	celebration.name.Assign("Door Celebration");
	celebration.reentry = ReentryMode::Restart;
	TriggerDefinition signal_trigger;
	signal_trigger.kind = TriggerKind::Signal;
	signal_trigger.signal.Assign("door.opened");
	celebration.triggers.push_back(std::move(signal_trigger));

	TriggerDefinition stop_signal_trigger;
	stop_signal_trigger.kind = TriggerKind::Signal;
	stop_signal_trigger.signal.Assign("celebration.stop");
	celebration.stop_triggers.push_back(std::move(stop_signal_trigger));

	auto celebration_audio{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& audio_action{ std::get<ActionItem>(celebration_audio.data).action };
	audio_action = MakeAction(ActionKind::PlayAudio);
	std::get<PlayAudioParams>(audio_action.parameters).asset.Assign("success_chime");
	celebration.sequence.push_back(std::move(celebration_audio));

	auto celebration_rotate{ MakeSequenceItem(SequenceItemKind::TimedAction) };
	auto& rotate{ std::get<TimedActionItem>(celebration_rotate.data) };
	rotate.action = MakeAction(ActionKind::RotateTo);
	std::get<RotateToParams>(rotate.action.parameters).degrees = 360.0f;
	rotate.duration_ms = 750.0f;
	rotate.ease = Ease::OutBack;
	celebration.sequence.push_back(std::move(celebration_rotate));

	auto celebration_wait{ MakeSequenceItem(SequenceItemKind::Wait) };
	std::get<WaitItem>(celebration_wait.data).duration_ms = 250.0f;
	celebration.sequence.push_back(std::move(celebration_wait));

	LifecycleCallbackDefinition celebration_complete;
	celebration_complete.event = LifecycleEventKind::Complete;
	celebration_complete.kind  = LifecycleCallbackKind::EmitSignal;
	celebration_complete.signal.Assign("celebration.finished");
	celebration.lifecycle_callbacks.push_back(std::move(celebration_complete));

	LifecycleCallbackDefinition celebration_yoyo;
	celebration_yoyo.event	= LifecycleEventKind::Yoyo;
	celebration_yoyo.kind	= LifecycleCallbackKind::Action;
	celebration_yoyo.action = MakeAction(ActionKind::PlayAudio);
	std::get<PlayAudioParams>(celebration_yoyo.action.parameters).asset.Assign("yoyo_tick");
	celebration.lifecycle_callbacks.push_back(std::move(celebration_yoyo));

	const Id celebration_id{ celebration.id };
	registry.definitions.push_back(std::move(celebration));

	BehaviorDefinition damage_flash;
	damage_flash.name.Assign("Damage Flash");
	damage_flash.reentry = ReentryMode::Restart;
	damage_flash.destroy_on_complete = true;
	TriggerDefinition damaged;
	damaged.kind = TriggerKind::Signal;
	damaged.signal.Assign("player.damaged");
	damage_flash.triggers.push_back(std::move(damaged));
	auto damage_action_item{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& damage_action{ std::get<ActionItem>(damage_action_item.data).action };
	damage_action = MakeAction(ActionKind::ApplyDamage);
	std::get<ApplyDamageParams>(damage_action.parameters).amount = 25.0f;
	damage_flash.sequence.push_back(std::move(damage_action_item));
	const Id damage_id{ damage_flash.id };
	registry.definitions.push_back(std::move(damage_flash));

	std::vector<EntityData> entities;

	EntityData door;
	door.name.Assign("Door");
	door.tag.Assign("Interactable");
	door.position[0] = 120.0f;
	door.position[1] = 40.0f;
	door.behaviors.emplace();
	BehaviorBinding open_door{ MakeLocalBinding() };
	auto& open{ open_door.local_definition };
	open.name.Assign("Open Door");
	open.triggers.clear();
	open.sequence.clear();
	TriggerDefinition overlap;
	overlap.kind = TriggerKind::OverlapStart;
	overlap.tag_filter.Assign("Player");
	open.triggers.push_back(std::move(overlap));

	auto move{ MakeSequenceItem(SequenceItemKind::TimedAction) };
	auto& timed_move{ std::get<TimedActionItem>(move.data) };
	timed_move.action = MakeAction(ActionKind::MoveTo);
	auto& move_params{ std::get<MoveToParams>(timed_move.action.parameters) };
	move_params.destination[1] = 64.0f;
	timed_move.duration_ms = 300.0f;
	timed_move.ease = Ease::OutCubic;
	open.sequence.push_back(std::move(move));

	auto wait{ MakeSequenceItem(SequenceItemKind::Wait) };
	std::get<WaitItem>(wait.data).duration_ms = 100.0f;
	open.sequence.push_back(std::move(wait));

	auto collider{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& collider_action{ std::get<ActionItem>(collider.data).action };
	collider_action = MakeAction(ActionKind::SetColliderMode);
	std::get<SetColliderModeParams>(collider_action.parameters).mode = 0;
	open.sequence.push_back(std::move(collider));

	auto audio{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& door_audio{ std::get<ActionItem>(audio.data).action };
	door_audio = MakeAction(ActionKind::PlayAudio);
	std::get<PlayAudioParams>(door_audio.parameters).asset.Assign("door_open");
	open.sequence.push_back(std::move(audio));

	auto emit{ MakeSequenceItem(SequenceItemKind::EmitSignal) };
	std::get<EmitSignalItem>(emit.data).signal.Assign("door.opened");
	open.sequence.push_back(std::move(emit));
	door.behaviors->bindings.push_back(std::move(open_door));
	entities.push_back(std::move(door));

	EntityData light;
	light.name.Assign("Celebration Light");
	light.tag.Assign("Decoration");
	light.position[0] = 260.0f;
	light.position[1] = 40.0f;
	light.behaviors.emplace();
	light.behaviors->bindings.push_back(MakeGlobalBinding(celebration_id));
	entities.push_back(std::move(light));

	EntityData player;
	player.name.Assign("Player");
	player.tag.Assign("Player");
	player.position[0] = -60.0f;
	player.position[1] = 40.0f;
	player.behaviors.emplace();
	player.behaviors->bindings.push_back(MakeGlobalBinding(damage_id));
	entities.push_back(std::move(player));

	EntityData factory;
	factory.name.Assign("Runtime Factory");
	factory.tag.Assign("Spawner");
	factory.position[0] = -240.0f;
	factory.position[1] = -90.0f;
	factory.behaviors.emplace();

	BehaviorBinding entity_tools{ MakeLocalBinding() };
	auto& tools{ entity_tools.local_definition };
	tools.name.Assign("Runtime Entity Tools");
	tools.triggers.clear();
	tools.sequence.clear();

	auto spawn_zombie{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& spawn_action{ std::get<ActionItem>(spawn_zombie.data).action };
	spawn_action = MakeAction(ActionKind::SpawnEntity);
	auto& spawn_params{ std::get<SpawnEntityParams>(spawn_action.parameters) };
	spawn_params.prefab_key.Assign("prefabs/zombie");
	spawn_params.count			 = 3;
	spawn_params.area			 = SpawnArea::Circle;
	spawn_params.radius			 = 72.0f;
	spawn_params.random_rotation = true;
	tools.sequence.push_back(std::move(spawn_zombie));

	auto add_health{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& add_health_action{ std::get<ActionItem>(add_health.data).action };
	add_health_action = MakeAction(ActionKind::AddComponent);
	auto& add_components{ std::get<AddComponentParams>(add_health_action.parameters).components };
	add_components.clear();
	auto health_payload{ MakeComponent(ComponentKind::Health) };
	std::get<HealthComponentData>(health_payload.data).maximum = 200.0f;
	std::get<HealthComponentData>(health_payload.data).current = 200.0f;
	add_components.push_back(std::move(health_payload));
	auto visible_payload{ MakeComponent(ComponentKind::Visible) };
	std::get<VisibleComponentData>(visible_payload.data).visible = true;
	add_components.push_back(std::move(visible_payload));
	tools.sequence.push_back(std::move(add_health));

	auto remove_damage{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& remove_damage_action{ std::get<ActionItem>(remove_damage.data).action };
	remove_damage_action = MakeAction(ActionKind::RemoveComponent);
	std::get<RemoveComponentParams>(remove_damage_action.parameters).components = {
		ComponentKind::Damage, ComponentKind::Lifetime
	};
	tools.sequence.push_back(std::move(remove_damage));

	factory.behaviors->bindings.push_back(std::move(entity_tools));
	entities.push_back(std::move(factory));

	EntityData camera;
	camera.name.Assign("Main Camera");
	camera.tag.Assign("Camera");
	entities.push_back(std::move(camera));
	return entities;
}

void DrawHierarchy(
	std::vector<EntityData>& entities, int& selected_index, bool& inspect_prefab
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
		EntityData entity;
		entity.name.Assign("New Entity");
		entities.push_back(std::move(entity));
		selected_index = static_cast<int>(entities.size()) - 1;
		inspect_prefab = false;
	}
}

void DrawSidebar(
	std::vector<EntityData>& entities, int& selected_entity, PrefabRegistry& prefabs,
	int& selected_prefab, bool& inspect_prefab
) {
	if (ImGui::BeginTabBar("SidebarTabs")) {
		if (ImGui::BeginTabItem("Scene")) {
			DrawHierarchy(entities, selected_entity, inspect_prefab);
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
		start, ImVec2{ start.x + size.x, start.y + size.y },
		ImGui::GetColorU32(ImGuiCol_FrameBg)
	);
	const ImVec2 center{ start.x + size.x * 0.5f, start.y + size.y * 0.5f };

	for (int i{ 0 }; i < static_cast<int>(entities.size()); ++i) {
		const auto& entity{ entities[static_cast<std::size_t>(i)] };
		const ImVec2 p{ center.x + entity.position[0], center.y - entity.position[1] };
		const ImU32 color{
			ImGui::GetColorU32(i == selected_index ? ImGuiCol_ButtonHovered : ImGuiCol_Button)
		};
		draw->AddRectFilled(
			ImVec2{ p.x - 48.0f, p.y - 18.0f },
			ImVec2{ p.x + 48.0f, p.y + 18.0f }, color, 3.0f
		);
		draw->AddText(
			ImVec2{ p.x - 42.0f, p.y - 6.0f },
			ImGui::GetColorU32(ImGuiCol_Text), entity.name.Data()
		);
	}

	draw->AddText(
		ImVec2{ start.x + 10.0f, start.y + 10.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Use the Prefabs tab to author serialized component templates. Select Runtime Factory to "
		"see spawn, delete, add-component, and remove-component actions."
	);
	ImGui::InvisibleButton("SceneCanvas", size);
}

void DrawApplication(
	std::vector<EntityData>& entities,
	int& selected_entity,
	GlobalBehaviorRegistry& registry,
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
	ImGui::Begin("Behavior Component Inspector Demo", nullptr, flags);

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
			entities, selected_entity, prefabs, selected_prefab, inspect_prefab
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

	GLFWwindow* window{ glfwCreateWindow(1650, 950, "Protegon Behaviors and Prefabs Demo", nullptr, nullptr) };
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

	demo::GlobalBehaviorRegistry registry;
	auto entities{ demo::MakeDemoEntities(registry) };
	demo::PrefabRegistry prefabs{ demo::MakeDemoPrefabs() };
	demo::DemoRuntimeContext runtime;
	int selected_entity{ 0 };
	int selected_prefab{ -1 };
	bool inspect_prefab{ false };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		const float dt{ ImGui::GetIO().DeltaTime };
		for (auto& entity : entities) {
			if (!entity.behaviors) {
				continue;
			}
			for (auto& binding : entity.behaviors->bindings) {
				demo::UpdateBehaviorRuntime(entity, binding, dt, registry, runtime);
			}
		}
		demo::DispatchSignals(entities, registry, runtime);
		demo::UpdateActivity(runtime, dt);
		demo::DrawApplication(
			entities, selected_entity, registry, runtime, prefabs, selected_prefab,
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
