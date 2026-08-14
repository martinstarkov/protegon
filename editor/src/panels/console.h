#pragma once

#include <cstdint>
#include <string>

namespace ptgn::editor {

class EditorContext;

class ConsolePanel {
public:
	void OnRender(EditorContext& ctx);

private:
	std::uint64_t last_rendered_output_revision_{ 0 };
	std::string rendered_output_;
	std::string save_status_;
	bool follow_output_tail_{ true };
	bool jump_to_bottom_requested_{ true };
};

} // namespace ptgn::editor
