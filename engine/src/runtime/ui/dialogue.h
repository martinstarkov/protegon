#pragma once

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
#include "runtime/scripting/script.h"
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
	Background,
	Text
};
PTGN_REFLECT_ENUM(DialoguePartRole);

namespace impl {

struct DialoguePart {
	DialoguePartRole role{ DialoguePartRole::Text };

	PTGN_REFLECT(DialoguePart, role)
};

struct DialogueWaitScript final : Script {
	void OnEvent(Event event) override;

	PTGN_REFLECT_EMPTY(DialogueWaitScript)

private:
	void OnKeyPressed(Key key);
	void OnKeyReleased(Key key);

	std::vector<Key> held_keys_{};
};

struct DialogueScrollScript final : Script {
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;

	PTGN_REFLECT_EMPTY(DialogueScrollScript)
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

struct DialoguePage {
	StyledText styled_text{};
	DialoguePageProperties properties{};

	/// @brief If true, this page skips the typewriter reveal and appears immediately.
	bool instant{ false };

	DialoguePage() = default;
	DialoguePage(StyledText styled_text, const DialoguePageProperties& properties);
};

namespace impl {

/// @brief Dialogue-only divider control. Put this on its own line between pages to make the
/// following page appear immediately even when Typewriter Text is enabled.
inline constexpr std::string_view kDialogueInstantPageTag{ "[[instant]]" };

/// @brief Dialogue-specific authoring pagination. A blank source line (two or more real
/// newlines) creates a manual page break. A single real newline and the two-character
/// sequence \\n are in-page line breaks before ordinary text pagination.
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

struct DialogueVariant {
	std::vector<DialoguePage> pages{};
};

struct DialogueEntry {
	std::size_t initial_variant{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next_dialogue{};

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

	DialogueMap dialogues{};

	/// @brief Canonical authoring definition serialized by the component.
	/// Runtime pages are rebuilt from this JSON and are never written back into scene files.
	json definition = json::object();

	/// @brief Runtime-only invalidation flag. Editor changes update Definition and mark compiled
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

	// Convenience compatibility for single-key callers.
	Key GetContinueKey() const;
	DialogueBox& SetContinueKey(Key continue_key);

	bool IsOpen() const;

	DialogueBox& Open(std::string_view dialogue_name = {});
	DialogueBox& Close();

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
	friend struct impl::DialogueWaitScript;
	friend struct impl::DialogueScrollScript;

	[[nodiscard]] std::optional<Entity> TryPart(DialoguePartRole role) const;
	Entity Part(DialoguePartRole role);

	void EnsureRuntimeData();
	void ApplyCurrentPage();
	void StartCurrentPageScroll();
	void StopCurrentPageScroll();
	void PositionTextForPage(const DialoguePageProperties& properties);
};

DialogueBox CreateDialogueBox(Scene& scene, Transform transform, const DialogueDesc& desc);

void to_json(json& j, const DialoguePageProperties& properties);
void from_json(const json& j, DialoguePageProperties& properties);

void to_json(json& j, const DialoguePage& page);
void from_json(const json& j, DialoguePage& page);

void to_json(json& j, const DialogueVariant& variant);
void from_json(const json& j, DialogueVariant& variant);

void to_json(json& j, const DialogueEntry& dialogue);
void from_json(const json& j, DialogueEntry& dialogue);

void to_json(json& j, const DialogueData& data);
void from_json(const json& j, DialogueData& data);

} // namespace ptgn
