#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace ptgn::editor {

class EditorCommand {
public:
	virtual ~EditorCommand() = default;

	virtual void Undo() = 0;
	virtual void Redo() = 0;

	[[nodiscard]] virtual std::string_view Label() const = 0;
};

class ActionEditorCommand final : public EditorCommand {
public:
	using Action = std::function<void()>;

	ActionEditorCommand(std::string label, Action undo, Action redo) :
		label_{ std::move(label) }, undo_{ std::move(undo) }, redo_{ std::move(redo) } {}

	void Undo() override {
		if (undo_) {
			undo_();
		}
	}

	void Redo() override {
		if (redo_) {
			redo_();
		}
	}

	[[nodiscard]] std::string_view Label() const override {
		return label_;
	}

private:
	std::string label_;
	Action undo_;
	Action redo_;
};

} // namespace ptgn::editor
