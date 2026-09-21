#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "core/util/time.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/ui/button_config.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class AssetManager;
class DialogueBox;
class Scene;

enum class DialogueBehavior {
	Sequential,
	Random
};
PTGN_REFLECT_ENUM(DialogueBehavior);

enum class DialoguePartRole : std::uint8_t {
	// Keep the first two values stable for existing serialized scenes.
	Background,
	Text,
	BackgroundOverride,
	Border,
	Sprite,
	PortraitLeft,
	PortraitCenter,
	PortraitRight
};
PTGN_REFLECT_ENUM(DialoguePartRole);

enum class DialoguePortraitSlot : std::uint8_t {
	Left,
	Center,
	Right
};
PTGN_REFLECT_ENUM(DialoguePortraitSlot);

namespace impl {

struct DialoguePart {
	DialoguePartRole role{ DialoguePartRole::Text };

	PTGN_REFLECT(DialoguePart, role)
};

struct DialogueSystem {
	/// @brief Handles the dialogue continue key expression for every DialogueData entity.
	static void OnEvent(Scene& scene, Event event);

	/// @brief Advances active typewriter reveals without using the scripting runtime.
	static void Update(Scene& scene, secondsf delta_time);

private:
	static void OnKeyPressed(Scene& scene, Key key);
	static void OnKeyReleased(Scene& scene, Key key);
};

} // namespace impl

struct DialoguePageProperties {
	DialoguePageProperties() = default;

	bool operator==(const DialoguePageProperties&) const = default;

	[[nodiscard]] DialoguePageProperties InheritProperties(const json& j) const;

	[[nodiscard]] V2_float TextAreaSize() const;
	[[nodiscard]] Rect TextAreaRect() const;
	[[nodiscard]] TextBox ToTextBox() const;
	void ApplyToText(Text text) const;

	TextRunDefaults text_defaults{};

	V2_float box_size{};
	Padding padding{};

	milliseconds scroll_duration{ 1000 };

	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::Word };
	OverflowMode overflow_mode{ OverflowMode::Clip };
};

struct DialoguePortraitCue {
	DialoguePortraitSlot slot{ DialoguePortraitSlot::Left };
	/// Empty speaker hides this slot.
	std::string speaker{};
	/// Empty expression resolves to the speaker's default expression.
	std::string expression{};

	bool operator==(const DialoguePortraitCue&) const = default;
};

struct DialoguePage {
	StyledText styled_text{};
	DialoguePageProperties properties{};

	/// @brief If true, this page skips the typewriter reveal and appears immediately.
	bool instant{ false };

	/// @brief Stateful portrait changes applied when this page becomes current.
	std::vector<DialoguePortraitCue> portrait_cues{};
	/// @brief Last authored non hidden portrait cue on this page. If absent, the previous speaker stays active.
	std::optional<DialoguePortraitSlot> speaking_slot{};

	DialoguePage() = default;
	DialoguePage(StyledText styled_text, const DialoguePageProperties& properties);
};

namespace impl {

/// @brief Dialogue only divider control. Put this on its own line between pages to make the
/// following page appear immediately even when Typewriter Text is enabled.
inline constexpr std::string_view kDialogueInstantPageTag{ "[[instant]]" };

/// @brief Prefix/suffix for a standalone page duration override, e.g. [[duration=750ms]].
inline constexpr std::string_view kDialogueDurationPageTagPrefix{ "[[duration=" };
inline constexpr std::string_view kDialogueDurationPageTagSuffix{ "]]" };
inline constexpr std::string_view kDialoguePortraitPageTagPrefix{ "[[portrait=" };
inline constexpr std::string_view kDialoguePortraitPageTagSuffix{ "]]" };

/// @brief Dialogue specific authoring pagination. A blank source line (two or more real
/// newlines) creates a manual page break. A single real newline and the two character
/// sequence `\\n` are in page line breaks before ordinary text pagination.
[[nodiscard]] std::vector<DialoguePage> PaginateDialogueSource(
	AssetManager& asset_manager, std::string_view source,
	const DialoguePageProperties& properties, std::string_view split_end = "...",
	std::string_view split_begin = {}
);

[[nodiscard]] std::string DialogueKeyName(Key key);
[[nodiscard]] bool ValidateDialogueKeyExpression(
	std::string_view expression, std::string* error = nullptr
);
[[nodiscard]] bool DialogueKeyExpressionMatches(
	std::string_view expression, std::span<const Key> held_keys
);

} // namespace impl


struct DialoguePortraitExpression {
	std::string display_name{};
	ButtonSpriteVisual idle{};
	std::optional<ButtonSpriteVisual> talking{};
};

struct DialoguePortraitActor {
	std::string display_name{};
	std::string default_expression{};
	std::unordered_map<
		std::string, DialoguePortraitExpression, StringHash, std::equal_to<>
	> expressions{};
};

using DialoguePortraitActorMap = std::unordered_map<
	std::string, DialoguePortraitActor, StringHash, std::equal_to<>
>;

struct DialoguePortraitRuntimeState {
	std::string speaker{};
	std::string expression{};
};

/// @brief Dialogue key appearance/audio overrides. Empty optionals inherit the dialogue entity's
/// ordinary background and have no extra border/sprite/audio behavior.
struct DialogueSounds {
	std::optional<AudioKey> open{};
	std::optional<AudioKey> typewriter{};

	PTGN_REFLECT(DialogueSounds, open, typewriter)
};

struct DialogueAppearance {
	std::optional<ButtonShapeVisual> background{};
	std::optional<ButtonShapeVisual> border{};
	std::optional<ButtonSpriteVisual> sprite{};
	std::optional<DialogueSounds> audio{};

	[[nodiscard]] bool Empty() const {
		return !background.has_value() && !border.has_value() &&
			!sprite.has_value() && !audio.has_value();
	}

	PTGN_REFLECT(DialogueAppearance, background, border, sprite, audio)
};

struct DialogueVariant {
	std::vector<DialoguePage> pages{};
};

struct DialogueEntry {
	std::size_t initial_variant{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next_dialogue{};
	DialogueAppearance appearance{};

	std::vector<DialogueVariant> variants{};

	std::size_t variant_cursor{ 0 };
	std::vector<std::size_t> used_variant_indices{};
	bool opened{ false };

	void ResetRuntimeState();
	[[nodiscard]] std::size_t PickRandomVariantIndex() const;
	const DialogueVariant* GetCurrentDialogueVariant() const;
	std::optional<std::size_t> GetNewDialogueVariant();
};

using DialogueMap = std::unordered_map<std::string, DialogueEntry, StringHash, std::equal_to<>>;

struct DialogueData {
	std::string continue_keys{ "Enter" };

	std::size_t current_variant{ 0 };
	std::size_t current_page{ 0 };
	std::string current_dialogue{};

	bool open{ false };

	/// @brief Runtime only state owned by DialogueSystem.
	std::vector<Key> held_continue_keys{};
	bool scrolling{ false };
	float scroll_elapsed_ms{ 0.0f };
	std::size_t revealed_character_count{ 0 };
	bool page_complete{ false };

	DialogueMap dialogues{};
	DialoguePortraitActorMap portrait_actors{};

	/// @brief Runtime only portrait state. Authored page cues update these slots incrementally.
	std::array<std::optional<DialoguePortraitRuntimeState>, 3> portrait_states{};
	std::optional<DialoguePortraitSlot> current_speaking_slot{};

	/// @brief Canonical authoring definition serialized by the component.
	/// Runtime pages are rebuilt from this JSON and are never written back into scene files.
	json definition = json::object();

	/// @brief Runtime only invalidation flag. Editor changes update Definition and mark compiled
	/// variants dirty; gameplay recompiles them before the dialogue is opened.
	bool runtime_dirty{ true };

	[[nodiscard]] static json MakeDefaultDefinition();

	[[nodiscard]] const json& Definition() const;
	void SetDefinition(json value);
	void MarkRuntimeDirty();

	void ClearRuntimeState();
	void RebuildRuntime(const Scene& scene);
	void LoadFromJson(
		const Scene& scene, const json& root, const DialoguePageProperties& default_properties
	);
};

struct DialogueDesc {
	Origin origin{ Origin::Center };

	json data = json::object();

	/// @brief Used when no background texture is supplied, and as a fallback if texture size cannot
	/// be resolved.
	V2_float box_size{};

	std::optional<TextureKey> background_texture{};
	Color background_color{ color::Black.WithAlpha(180) };

	bool ui_layer{ true };
};

class DialogueBox : public Entity {
public:
	DialogueBox() = default;
	explicit DialogueBox(Entity entity);

	[[nodiscard]] DialogueData& Data();
	[[nodiscard]] const DialogueData& Data() const;

	[[nodiscard]] std::string_view GetContinueKeys() const;
	DialogueBox& SetContinueKeys(std::string_view continue_keys);

	Key GetContinueKey() const;
	DialogueBox& SetContinueKey(Key continue_key);

	bool IsOpen() const;

	DialogueBox& Open(std::string_view dialogue_name = {});
	DialogueBox& Close();

	/// @brief Completes the current typewriter reveal, or advances when already complete.
	DialogueBox& Advance();
	DialogueBox& NextPage();
	DialogueBox& CompletePage();

	DialogueBox& SetDialogue(std::string_view name = {});
	DialogueBox& SetNextDialogue();

	DialogueEntry* GetCurrentDialogue();
	DialogueVariant* GetCurrentDialogueVariant();
	DialoguePage* GetCurrentDialoguePage();

	Text TextPart();
	[[nodiscard]] std::optional<Text> TryTextPart() const;

	[[nodiscard]] std::optional<Sprite> TryBackground() const;
	[[nodiscard]] std::optional<Entity> TryBackgroundEntity() const;

private:
	friend struct impl::DialogueSystem;

	[[nodiscard]] std::optional<Entity> TryPart(DialoguePartRole role) const;
	Entity Part(DialoguePartRole role);

	void EnsureRuntimeData();
	void ApplyCurrentPage();
	void ApplyCurrentAppearance(const DialoguePageProperties& properties);
	void ApplyCurrentPortraitCues();
	void RefreshPortraits(const DialoguePageProperties& properties, bool talking);
	void HidePortraits();
	void FinishPortraitTalking();
	void HideAppearanceOverrides();
	void PlayOpenSound();
	void PlayTypewriterSound();
	void StartCurrentPageScroll();
	void StopCurrentPageScroll();
	void PositionTextForPage(const DialoguePageProperties& properties);
};

namespace event {

struct DialogueOpened {
	operator DialogueBox() const { // NOSONAR
		return dialogue;
	}

	DialogueBox dialogue{};
	std::string key{};
	std::size_t variant{ 0 };
};

struct DialogueClosed {
	operator DialogueBox() const { // NOSONAR
		return dialogue;
	}

	DialogueBox dialogue{};
	std::string key{};
};

struct DialogueChanged {
	operator DialogueBox() const { // NOSONAR
		return dialogue;
	}

	DialogueBox dialogue{};
	std::string previous{};
	std::string current{};
};

struct DialoguePageChanged {
	operator DialogueBox() const { // NOSONAR
		return dialogue;
	}

	DialogueBox dialogue{};
	std::string key{};
	std::size_t variant{ 0 };
	std::size_t page{ 0 };
};

struct DialoguePageCompleted {
	operator DialogueBox() const { // NOSONAR
		return dialogue;
	}

	DialogueBox dialogue{};
	std::string key{};
	std::size_t variant{ 0 };
	std::size_t page{ 0 };
};

struct DialogueFinished {
	operator DialogueBox() const { // NOSONAR
		return dialogue;
	}

	DialogueBox dialogue{};
	std::string key{};
	std::size_t variant{ 0 };
};

} // namespace event

DialogueBox CreateDialogueBox(Scene& scene, Transform transform, const DialogueDesc& desc);

void to_json(json& j, const DialoguePageProperties& properties);
void from_json(const json& j, DialoguePageProperties& properties);

void to_json(json& j, const DialoguePortraitCue& cue);
void from_json(const json& j, DialoguePortraitCue& cue);
void to_json(json& j, const DialoguePage& page);
void from_json(const json& j, DialoguePage& page);

void to_json(json& j, const DialoguePortraitExpression& expression);
void from_json(const json& j, DialoguePortraitExpression& expression);
void to_json(json& j, const DialoguePortraitActor& actor);
void from_json(const json& j, DialoguePortraitActor& actor);

void to_json(json& j, const DialogueVariant& variant);
void from_json(const json& j, DialogueVariant& variant);

void to_json(json& j, const DialogueSounds& sounds);
void from_json(const json& j, DialogueSounds& sounds);
void to_json(json& j, const DialogueAppearance& appearance);
void from_json(const json& j, DialogueAppearance& appearance);

void to_json(json& j, const DialogueEntry& dialogue);
void from_json(const json& j, DialogueEntry& dialogue);

void to_json(json& j, const DialogueData& data);
void from_json(const json& j, DialogueData& data);

} // namespace ptgn
