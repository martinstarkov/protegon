#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace ptgn::editor {

namespace impl {

enum class OutputConsoleOpenTargetKind {
	Url,
	Path,
};

struct OutputConsoleOpenTarget {
	OutputConsoleOpenTargetKind kind{ OutputConsoleOpenTargetKind::Url };
	std::string value{};
};

[[nodiscard]] inline bool IsOutputConsoleWhitespace(char value) {
	return value == ' ' ||
		   value == '\t' ||
		   value == '\r' ||
		   value == '\n';
}

[[nodiscard]] inline std::string_view TrimOutputConsoleValue(
	std::string_view value
) {
	while (!value.empty() &&
		   IsOutputConsoleWhitespace(value.front())) {
		value.remove_prefix(1);
	}

	while (!value.empty() &&
		   IsOutputConsoleWhitespace(value.back())) {
		value.remove_suffix(1);
	}

	return value;
}

[[nodiscard]] inline bool IsUrlTrailingPunctuation(char value) {
	switch (value) {
		case '.':
		case ',':
		case ';':
		case ')':
		case ']':
		case '}':
		case '>':
		case '"':
		case '\'':
			return true;

		default:
			return false;
	}
}

[[nodiscard]] inline std::optional<OutputConsoleOpenTarget>
FindOutputConsoleUrlAtCursor(
	std::string_view line,
	std::size_t line_offset,
	std::size_t cursor
) {
	constexpr std::array<std::string_view, 3> schemes{
		"https://",
		"http://",
		"file://",
	};

	for (const auto scheme : schemes) {
		std::size_t search_offset{};

		while (search_offset < line.size()) {
			const auto begin{
				line.find(
					scheme,
					search_offset
				)
			};

			if (begin == std::string_view::npos) {
				break;
			}

			std::size_t end{ begin + scheme.size() };

			while (
				end < line.size() &&
				!IsOutputConsoleWhitespace(line[end])
			) {
				++end;
			}

			while (
				end > begin + scheme.size() &&
				IsUrlTrailingPunctuation(line[end - 1])
			) {
				--end;
			}

			const std::size_t absolute_begin{
				line_offset + begin
			};
			const std::size_t absolute_end{
				line_offset + end
			};

			if (
				cursor >= absolute_begin &&
				cursor < absolute_end
			) {
				return OutputConsoleOpenTarget{
					.kind = OutputConsoleOpenTargetKind::Url,
					.value = std::string{
						line.substr(
							begin,
							end - begin
						)
					},
				};
			}

			search_offset = std::max(
				end,
				begin + scheme.size()
			);
		}
	}

	return std::nullopt;
}

[[nodiscard]] inline std::optional<OutputConsoleOpenTarget>
FindOutputConsolePathAtCursor(
	std::string_view line,
	std::size_t line_offset,
	std::size_t cursor
) {
	// These are intentionally explicit rather than treating arbitrary console
	// text as a filesystem path. This keeps compiler/build output from opening
	// unintended locations.
	constexpr std::array<std::string_view, 5> prefixes{
		"Serving:",
		"Output:",
		"Build cache:",
		"Source:",
		"Directory:",
	};

	std::size_t leading_whitespace{};
	while (
		leading_whitespace < line.size() &&
		IsOutputConsoleWhitespace(
			line[leading_whitespace]
		)
	) {
		++leading_whitespace;
	}

	const std::string_view trimmed_line{
		line.substr(leading_whitespace)
	};

	for (const auto prefix : prefixes) {
		if (!trimmed_line.starts_with(prefix)) {
			continue;
		}

		std::size_t begin{
			leading_whitespace + prefix.size()
		};

		while (
			begin < line.size() &&
			IsOutputConsoleWhitespace(line[begin])
		) {
			++begin;
		}

		std::size_t end{ line.size() };

		while (
			end > begin &&
			IsOutputConsoleWhitespace(line[end - 1])
		) {
			--end;
		}

		if (
			end > begin + 1 &&
			(
				(line[begin] == '"' && line[end - 1] == '"') ||
				(line[begin] == '\'' && line[end - 1] == '\'')
			)
		) {
			++begin;
			--end;
		}

		const std::size_t absolute_begin{
			line_offset + begin
		};
		const std::size_t absolute_end{
			line_offset + end
		};

		if (
			cursor < absolute_begin ||
			cursor >= absolute_end
		) {
			continue;
		}

		const std::string_view path_text{
			TrimOutputConsoleValue(
				line.substr(
					begin,
					end - begin
				)
			)
		};

		if (path_text.empty()) {
			return std::nullopt;
		}

		std::error_code error;
		const std::filesystem::path target_path{
			std::string{ path_text }
		};

		if (
			!std::filesystem::exists(
				target_path,
				error
			) ||
			error
		) {
			return std::nullopt;
		}

		return OutputConsoleOpenTarget{
			.kind = OutputConsoleOpenTargetKind::Path,
			.value = target_path.string(),
		};
	}

	return std::nullopt;
}

[[nodiscard]] inline std::optional<OutputConsoleOpenTarget>
FindOutputConsoleTargetAtCursor(
	std::string_view output,
	std::size_t cursor
) {
	if (output.empty()) {
		return std::nullopt;
	}

	cursor = std::min(
		cursor,
		output.size() - 1
	);

	const auto previous_newline{
		cursor == 0
			? std::string_view::npos
			: output.rfind(
				'\n',
				cursor - 1
			)
	};

	const std::size_t line_begin{
		previous_newline == std::string_view::npos
			? 0
			: previous_newline + 1
	};

	const auto next_newline{
		output.find(
			'\n',
			cursor
		)
	};

	const std::size_t line_end{
		next_newline == std::string_view::npos
			? output.size()
			: next_newline
	};

	const std::string_view line{
		output.substr(
			line_begin,
			line_end - line_begin
		)
	};

	if (auto url{
			FindOutputConsoleUrlAtCursor(
				line,
				line_begin,
				cursor
			)
		}) {
		return url;
	}

	return FindOutputConsolePathAtCursor(
		line,
		line_begin,
		cursor
	);
}

inline bool OpenOutputConsoleTarget(
	const OutputConsoleOpenTarget& target
) {
	auto& platform_io{
		ImGui::GetPlatformIO()
	};

	if (!platform_io.Platform_OpenInShellFn) {
		return false;
	}

	std::string value{ target.value };

	if (
		target.kind ==
		OutputConsoleOpenTargetKind::Path
	) {
		std::error_code error;
		std::filesystem::path target_path{
			target.value
		};

		if (
			std::filesystem::is_regular_file(
				target_path,
				error
			) &&
			!error
		) {
			// For files, open their containing directory rather than launching
			// the file itself. Directory paths are opened directly.
			target_path =
				target_path.parent_path();

			if (target_path.empty()) {
				return false;
			}

			value = target_path.string();
		} else {
			error.clear();

			if (
				!std::filesystem::is_directory(
					target_path,
					error
				) ||
				error
			) {
				return false;
			}
		}
	}

	return platform_io.Platform_OpenInShellFn(
		ImGui::GetCurrentContext(),
		value.c_str()
	);
}

} // namespace impl

struct OutputConsoleActions {
	bool clear_requested{ false };
	bool save_requested{ false };
};

[[nodiscard]] inline OutputConsoleActions DrawOutputConsole(
	const char* id,
	std::string& output,
	bool& follow_tail,
	bool& jump_to_bottom_requested,
	bool show_save_button = false
) {
	OutputConsoleActions actions;

	ImGui::PushID(id);

	if (ImGui::Button("Clear Output")) {
		output.clear();
		follow_tail = true;
		jump_to_bottom_requested = true;
		actions.clear_requested = true;
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(output.empty());

	if (ImGui::Button("Copy All")) {
		ImGui::SetClipboardText(output.c_str());
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(output.empty());

	if (ImGui::Button("Jump to Bottom")) {
		follow_tail = true;
		jump_to_bottom_requested = true;
	}

	ImGui::EndDisabled();

	if (show_save_button) {
		ImGui::SameLine();
		ImGui::BeginDisabled(output.empty());

		if (ImGui::Button("Save Log")) {
			actions.save_requested = true;
		}

		ImGui::EndDisabled();
	}

	std::string selectable_output{ output };

	if (
		!selectable_output.empty() &&
		selectable_output.back() == '\n'
	) {
		selectable_output.pop_back();
	}

	ImGuiWindow* parent_window{
		ImGui::GetCurrentWindow()
	};

	const ImGuiID output_id{
		parent_window->GetID("##OutputText")
	};

	ImGui::InputTextMultiline(
		"##OutputText",
		selectable_output.data(),
		selectable_output.size() + 1,
		ImGui::GetContentRegionAvail(),
		ImGuiInputTextFlags_ReadOnly |
			ImGuiInputTextFlags_NoHorizontalScroll |
			ImGuiInputTextFlags_WordWrap
	);

	ImGuiWindow* output_window{ nullptr };

	for (ImGuiWindow* child : parent_window->DC.ChildWindows) {
		if (
			child &&
			child->ChildId == output_id
		) {
			output_window = child;
			break;
		}
	}

	if (output_window) {
		ImGuiContext& imgui_context{ *GImGui };
		const auto& io{ ImGui::GetIO() };

		constexpr float kBottomTolerance{ 2.0f };

		const ImGuiID vertical_scrollbar_id{
			ImGui::GetWindowScrollbarID(
				output_window,
				ImGuiAxis_Y
			)
		};

		const bool output_hovered{
			imgui_context.HoveredWindow ==
				output_window
		};

		const bool user_scrolled_up_with_wheel{
			output_hovered &&
			io.MouseWheel > 0.0f
		};

		const bool user_dragging_vertical_scrollbar{
			imgui_context.ActiveId ==
				vertical_scrollbar_id
		};

		if (
			!jump_to_bottom_requested &&
			(
				user_scrolled_up_with_wheel ||
				user_dragging_vertical_scrollbar
			)
		) {
			follow_tail = false;
		}

		const bool at_bottom{
			output_window->ScrollMax.y <=
				kBottomTolerance ||
			output_window->Scroll.y >=
				output_window->ScrollMax.y -
					kBottomTolerance
		};

		if (
			!follow_tail &&
			!user_dragging_vertical_scrollbar &&
			!user_scrolled_up_with_wheel &&
			at_bottom
		) {
			follow_tail = true;
		}

		if (
			follow_tail ||
			jump_to_bottom_requested
		) {
			output_window->Scroll.y =
				output_window->ScrollMax.y;

			ImGui::SetScrollY(
				output_window,
				FLT_MAX
			);

			follow_tail = true;
			jump_to_bottom_requested = false;
		}

#if defined(__APPLE__)
		const bool open_modifier{
			io.KeyCtrl || io.KeySuper
		};
#else
		const bool open_modifier{
			io.KeyCtrl
		};
#endif

		if (
			output_hovered &&
			open_modifier
		) {
			ImGui::SetMouseCursor(
				ImGuiMouseCursor_Hand
			);

			ImGui::SetTooltip(
#if defined(__APPLE__)
				"Ctrl/Cmd+Click a URL or exported path to open it."
#else
				"Ctrl+Click a URL or exported path to open it."
#endif
			);
		}

		const bool open_clicked{
			output_hovered &&
			open_modifier &&
			ImGui::IsMouseClicked(
				ImGuiMouseButton_Left
			) &&
			imgui_context.ActiveId ==
				output_id
		};

		if (open_clicked) {
			if (
				ImGuiInputTextState* input_state{
					ImGui::GetInputTextState(
						output_id
					)
				}
			) {
				const int cursor_position{
					input_state->GetCursorPos()
				};

				if (
					cursor_position >= 0
				) {
					if (
						auto target{
							impl::FindOutputConsoleTargetAtCursor(
								selectable_output,
								static_cast<std::size_t>(
									cursor_position
								)
							)
						}
					) {
						impl::OpenOutputConsoleTarget(
							target.value()
						);
					}
				}
			}
		}
	}

	ImGui::PopID();

	return actions;
}

} // namespace ptgn::editor
