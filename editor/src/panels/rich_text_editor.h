#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

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

	/// @brief Optional standalone source-line control exposed immediately after the Effects combo.
	/// The control is toggled on the selected blank divider, or inserted above the selected paragraph.
	std::string_view standalone_line_tag{};
	std::string_view standalone_line_button_label{};
	std::string_view standalone_line_tooltip{};
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
