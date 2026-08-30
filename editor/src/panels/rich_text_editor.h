#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ptgn {

struct TextRunDefaults;

} // namespace ptgn

namespace ptgn::editor {

class EditorContext;

namespace inspector {

struct RichTextVariableOption {
	std::string_view label{};
	std::string_view variable{};
	std::string_view preview{};
};

struct RichTextEditorOptions {
	std::span<const RichTextVariableOption> variables{};
	bool show_preview{ true };
	int line_count{ 8 };
};

/// @brief Unified rich-text source editor used by text components and context-specific UI.
bool DrawRichTextEditor(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options = {}
);

} // namespace inspector

} // namespace ptgn::editor
