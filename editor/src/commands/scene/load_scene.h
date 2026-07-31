#pragma once

#include <string>

#include "commands/editor_command.h"
#include "runtime/scene/scene_file.h"

namespace ptgn::editor {

class EditorContext;

class LoadSceneCommand final : public EditorCommand {
public:
	LoadSceneCommand(
		EditorContext& ctx,
		std::string scene_key,
		bool runtime,
		SerializedScene before,
		SerializedScene after
	);

	void Undo() override;
	void Redo() override;

	[[nodiscard]] std::string_view Label() const override;

private:
	void Apply(const SerializedScene& scene) const;

	EditorContext* ctx_{ nullptr };
	std::string scene_key_;
	bool runtime_{ false };
	SerializedScene before_;
	SerializedScene after_;
};

} // namespace ptgn::editor
