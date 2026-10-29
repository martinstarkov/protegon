#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "commands/editor_command.h"
#include "commands/entity/entity_reference.h"

namespace ptgn::editor {

class Editor;

template <typename T>
class ComponentStateCommand : public EditorCommand {
public:
	using State = std::optional<T>;

	ComponentStateCommand(
		Editor& editor,
		EntityReference entity,
		State before,
		State after,
		std::string label
	) :
		editor_{ &editor },
		entity_{ std::move(entity) },
		before_{ std::move(before) },
		after_{ std::move(after) },
		label_{ std::move(label) } {}

	void Undo() override {
		Apply(before_);
	}

	void Redo() override {
		Apply(after_);
	}

	[[nodiscard]] std::string_view Label() const override {
		return label_;
	}

private:
	void Apply(const State& state) const {
		Entity entity{ entity_.Resolve(*editor_) };
		if (!entity) {
			return;
		}

		if (state) {
			if (entity.template Has<T>()) {
				entity.template Get<T>() = *state;
			} else {
				entity.template Add<T>(*state);
			}
		} else if (entity.template Has<T>()) {
			entity.template Remove<T>();
		}
	}

	Editor* editor_{ nullptr };
	EntityReference entity_;
	State before_;
	State after_;
	std::string label_;
};

} // namespace ptgn::editor
