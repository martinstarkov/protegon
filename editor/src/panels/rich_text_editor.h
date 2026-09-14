#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ptgn {

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
	/// Useful for context editors that need source-aware actions such as splitting a dialogue page.
	RichTextEditorSelection* selection{ nullptr };
};

/// @brief Unified rich-text source editor used by text components and context-specific UI.
bool DrawRichTextEditor(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options = {}
);

} // namespace inspector

} // namespace ptgn::editor
