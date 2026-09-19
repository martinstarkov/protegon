#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "core/util/time.h"

namespace ptgn {

struct StyledText;
struct TextRunDefaults;
struct TextBox;

} // namespace ptgn

namespace ptgn::editor {

class EditorContext;

namespace inspector {

struct RichTextVariableOption {
	std::string_view label{};
	std::string_view variable{};
	std::string_view preview{};
};

struct RichTextPortraitExpressionOption {
	std::string_view key{};
	std::string_view label{};
};

struct RichTextPortraitSpeakerOption {
	std::string_view key{};
	std::string_view label{};
	std::string_view default_expression{};
	std::span<const RichTextPortraitExpressionOption> expressions{};
};

struct RichTextEditorSelection {
	std::size_t cursor{ 0 };
	std::size_t selection_start{ 0 };
	std::size_t selection_end{ 0 };
};

struct RichTextEditorOptions {
	std::span<const RichTextVariableOption> variables{};

	/// @brief Optional target text box used by the live preview. When supplied, wrapping,
	/// alignment, overflow, max-lines and clipping match the authored Text component.
	const TextBox* preview_box{ nullptr };

	bool show_preview{ true };
	int line_count{ 8 };

	/// @brief Optional readback of the current editor cursor/selection.
	RichTextEditorSelection* selection{ nullptr };

	/// @brief Hide the Defaults section when defaults are owned by a higher-level editor.
	/// Formatting toolbar tags remain available and continue to override the supplied defaults.
	bool show_defaults{ true };

	/// @brief Show the dialogue-only page-number overlay toggle in the toolbar.
	bool show_page_numbers_button{ false };

	/// @brief One-based dialogue page number for each logical displayed line. Zero suppresses
	/// the gutter label for that line, which is used for blank page-divider lines.
	std::span<const std::size_t> line_page_numbers{};

	/// @brief Optional compiled source shown while page-number mode is enabled. The editor becomes
	/// read-only in that mode; this text may therefore expose automatic pagination without changing
	/// the authored source.
	std::string_view page_number_preview_source{};

	/// @brief Optional generic standalone source-line control exposed immediately after Effects.
	std::string_view standalone_line_tag{};
	std::string_view standalone_line_button_label{};
	std::string_view standalone_line_tooltip{};

	/// @brief Dialogue page-duration authoring control. When enabled, a single Duration button
	/// manages either an instant marker or a timed [[duration=...]] marker for the selected page.
	bool show_page_duration_button{ false };
	std::string_view page_instant_tag{};
	std::string_view page_duration_tag_prefix{};
	std::string_view page_duration_tag_suffix{};
	milliseconds page_duration_default{ 1000 };

	/// @brief Dialogue portrait page control. Portrait metadata is authored as standalone lines.
	bool show_portrait_button{ false };
	std::span<const RichTextPortraitSpeakerOption> portrait_speakers{};
	std::string_view portrait_tag_prefix{};
	std::string_view portrait_tag_suffix{};
};

/// @brief Unified rich-text source editor used by text components and context-specific UI.
bool DrawRichTextEditor(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options = {}
);

/// @brief Draw only the renderer-backed rich-text preview surface.
/// Context-specific editors can use this when they need custom pagination/navigation.
void DrawRichTextPreview(
	EditorContext& ctx, const StyledText& styled_text, const TextBox* preview_box = nullptr
);

} // namespace inspector

} // namespace ptgn::editor
