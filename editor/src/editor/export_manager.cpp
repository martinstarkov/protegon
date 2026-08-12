#include "editor/export_manager.h"

#if !defined(__EMSCRIPTEN__)

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <regex>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "core/build_info.h"

namespace ptgn::editor {

namespace {

using namespace std::chrono_literals;

struct CopySource {
	path source;
	path destination;
};

[[nodiscard]] std::string TaskName(impl::ExportTaskKind kind) {
	switch (kind) {
		case impl::ExportTaskKind::Desktop: return "Desktop export";
		case impl::ExportTaskKind::Web: return "Web export";
		case impl::ExportTaskKind::Clean: return "Clean";
		case impl::ExportTaskKind::None: break;
	}
	return "Export";
}

[[nodiscard]] std::string_view ConfigurationName(
	ExportConfiguration configuration
) {
	return configuration == ExportConfiguration::Debug
		? std::string_view{ "Debug" }
		: std::string_view{ "Release" };
}

int ExportOutputTextCallback(ImGuiInputTextCallbackData* data) {
	auto* jump_to_bottom{ static_cast<bool*>(data->UserData) };
	if (!jump_to_bottom || !*jump_to_bottom) {
		return 0;
	}

	data->CursorPos = data->BufTextLen;
	data->SelectionStart = data->BufTextLen;
	data->SelectionEnd = data->BufTextLen;
	*jump_to_bottom = false;
	return 0;
}

[[nodiscard]] std::string QuotePosix(std::string_view value) {
	std::string result{ "'" };
	for (char c : value) {
		if (c == '\'') {
			result += "'\\''";
		} else {
			result.push_back(c);
		}
	}
	result.push_back('\'');
	return result;
}

[[nodiscard]] std::string QuoteWindows(std::string_view value) {
	std::string result{ "\"" };
	std::size_t backslashes{ 0 };
	for (char c : value) {
		if (c == '\\') {
			++backslashes;
			continue;
		}
		if (c == '"') {
			result.append(backslashes * 2 + 1, '\\');
			result.push_back('"');
			backslashes = 0;
			continue;
		}
		result.append(backslashes, '\\');
		backslashes = 0;
		result.push_back(c);
	}
	result.append(backslashes * 2, '\\');
	result.push_back('"');
	return result;
}

[[nodiscard]] std::string Quote(std::string_view value) {
#if defined(_WIN32)
	return QuoteWindows(value);
#else
	return QuotePosix(value);
#endif
}

[[nodiscard]] std::string MakeCommand(
	std::string_view executable,
	const std::vector<std::string>& arguments
) {
	std::string command{ Quote(executable) };
	for (const auto& argument : arguments) {
		command.push_back(' ');
		command += Quote(argument);
	}
	return command;
}

void AppendOutput(
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::string_view value
) {
	{
		std::scoped_lock lock{ state->output_mutex };
		state->output.append(value);
	}
	state->output_revision.fetch_add(1, std::memory_order_relaxed);
}

void AppendOutputLine(
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::string_view value
) {
	AppendOutput(state, value);
	AppendOutput(state, "\n");
}

void UpdateBuildProgressFromText(
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::string_view text,
	float base,
	float scale
) {
	static const std::regex progress_pattern{ R"(\[(\d+)\/(\d+))" };
	std::cmatch match;
	if (!std::regex_search(text.begin(), text.end(), match, progress_pattern)) {
		return;
	}

	const auto current{ static_cast<float>(std::strtoul(match[1].first, nullptr, 10)) };
	const auto total{ static_cast<float>(std::strtoul(match[2].first, nullptr, 10)) };
	if (total <= 0.0f) {
		return;
	}

	state->progress.store(
		std::clamp(base + scale * (current / total), 0.0f, 1.0f),
		std::memory_order_relaxed
	);
}

#if defined(_WIN32)

[[nodiscard]] int RunProcess(
	const std::string& command,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	SECURITY_ATTRIBUTES security_attributes{};
	security_attributes.nLength = sizeof(security_attributes);
	security_attributes.bInheritHandle = TRUE;

	HANDLE read_pipe{ nullptr };
	HANDLE write_pipe{ nullptr };
	if (!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0)) {
		AppendOutputLine(state, "Failed to create process output pipe.");
		return -1;
	}
	SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA startup_info{};
	startup_info.cb = sizeof(startup_info);
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdOutput = write_pipe;
	startup_info.hStdError = write_pipe;
	startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	PROCESS_INFORMATION process_info{};
	const std::string shell_command{ "cmd.exe /D /S /C \"" + command + "\"" };
	std::vector<char> mutable_command(shell_command.begin(), shell_command.end());
	mutable_command.push_back('\0');

	HANDLE job{ CreateJobObjectA(nullptr, nullptr) };
	if (!job) {
		CloseHandle(read_pipe);
		CloseHandle(write_pipe);
		AppendOutputLine(state, "Failed to create build process job.");
		return -1;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{};
	job_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(
		job,
		JobObjectExtendedLimitInformation,
		&job_info,
		sizeof(job_info)
	);

	const BOOL created{ CreateProcessA(
		nullptr,
		mutable_command.data(),
		nullptr,
		nullptr,
		TRUE,
		CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
		nullptr,
		nullptr,
		&startup_info,
		&process_info
	) };
	CloseHandle(write_pipe);

	if (!created) {
		CloseHandle(read_pipe);
		CloseHandle(job);
		AppendOutputLine(state, "Failed to start build process.");
		return -1;
	}

	AssignProcessToJobObject(job, process_info.hProcess);

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = reinterpret_cast<std::intptr_t>(process_info.hProcess);
		state->active_job = reinterpret_cast<std::intptr_t>(job);
	}

	if (state->cancel_requested.load(std::memory_order_relaxed)) {
		TerminateJobObject(job, 1);
	}

	std::array<char, 4096> buffer{};
	DWORD bytes_read{};
	while (ReadFile(
		read_pipe,
		buffer.data(),
		static_cast<DWORD>(buffer.size() - 1),
		&bytes_read,
		nullptr
	)) {
		if (bytes_read == 0) {
			break;
		}
		buffer[bytes_read] = '\0';
		AppendOutput(state, std::string_view{ buffer.data(), bytes_read });
		UpdateBuildProgressFromText(
			state,
			std::string_view{ buffer.data(), bytes_read },
			progress_base,
			progress_scale
		);
	}

	WaitForSingleObject(process_info.hProcess, INFINITE);
	DWORD exit_code{};
	GetExitCodeProcess(process_info.hProcess, &exit_code);

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = 0;
		state->active_job = 0;
	}

	CloseHandle(read_pipe);
	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);
	CloseHandle(job);
	return static_cast<int>(exit_code);
}

void TerminateActiveProcess(const std::shared_ptr<impl::ExportSharedState>& state) {
	std::scoped_lock lock{ state->process_mutex };
	if (state->active_job != 0) {
		TerminateJobObject(
			reinterpret_cast<HANDLE>(state->active_job),
			1
		);
		return;
	}
	if (state->active_process != 0) {
		TerminateProcess(
			reinterpret_cast<HANDLE>(state->active_process),
			1
		);
	}
}

#else

[[nodiscard]] int RunProcess(
	const std::string& command,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	int pipe_fds[2]{};
	if (pipe(pipe_fds) != 0) {
		AppendOutputLine(state, "Failed to create process output pipe.");
		return -1;
	}

	const pid_t pid{ fork() };
	if (pid < 0) {
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		AppendOutputLine(state, "Failed to fork build process.");
		return -1;
	}

	if (pid == 0) {
		setpgid(0, 0);
		dup2(pipe_fds[1], STDOUT_FILENO);
		dup2(pipe_fds[1], STDERR_FILENO);
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
		_exit(127);
	}

	setpgid(pid, pid);
	close(pipe_fds[1]);

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = static_cast<std::intptr_t>(pid);
	}

	if (state->cancel_requested.load(std::memory_order_relaxed)) {
		kill(-pid, SIGTERM);
		kill(-pid, SIGKILL);
	}

	FILE* stream{ fdopen(pipe_fds[0], "r") };
	if (stream) {
		std::array<char, 4096> buffer{};
		while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), stream)) {
			AppendOutput(state, buffer.data());
			UpdateBuildProgressFromText(
				state,
				buffer.data(),
				progress_base,
				progress_scale
			);
		}
		std::fclose(stream);
	} else {
		close(pipe_fds[0]);
	}

	int status{};
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
	}

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = 0;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		return 128 + WTERMSIG(status);
	}
	return -1;
}

void TerminateActiveProcess(const std::shared_ptr<impl::ExportSharedState>& state) {
	std::scoped_lock lock{ state->process_mutex };
	if (state->active_process == 0) {
		return;
	}

	const auto pid{ static_cast<pid_t>(state->active_process) };
	kill(-pid, SIGTERM);
	kill(-pid, SIGKILL);
}

#endif

[[nodiscard]] bool IsCancelled(const std::shared_ptr<impl::ExportSharedState>& state) {
	return state->cancel_requested.load(std::memory_order_relaxed);
}

[[nodiscard]] bool RunLoggedCommand(
	std::string_view executable,
	const std::vector<std::string>& arguments,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	if (IsCancelled(state)) {
		return false;
	}

	const std::string command{ MakeCommand(executable, arguments) };
	AppendOutputLine(state, "$ " + command);
	AppendOutputLine(state, "");

	const int result{ RunProcess(command, state, progress_base, progress_scale) };
	if (IsCancelled(state)) {
		return false;
	}
	if (result != 0) {
		AppendOutputLine(state, "");
		AppendOutputLine(state, "Command failed with exit code " + std::to_string(result) + ".");
		return false;
	}
	return true;
}


[[nodiscard]] bool IsEditorOnlyExportPath(const path& source) {
	return source.extension() == ".ptgnlocal";
}

[[nodiscard]] std::uintmax_t CountExportFiles(
	const path& source,
	const std::shared_ptr<impl::ExportSharedState>& state
) {
	std::uintmax_t count{};
	std::error_code error;

	if (fs::is_regular_file(source, error)) {
		return IsEditorOnlyExportPath(source) ? 1 : 1;
	}
	error.clear();

	if (!fs::is_directory(source, error)) {
		return 1;
	}

	for (
		fs::recursive_directory_iterator it{ source, error }, end;
		it != end && !error;
		it.increment(error)
	) {
		if (IsCancelled(state)) {
			break;
		}

		if (it->is_regular_file(error) &&
			!IsEditorOnlyExportPath(it->path())) {
			++count;
		}
		error.clear();
	}

	return std::max<std::uintmax_t>(count, 1);
}

[[nodiscard]] bool CopyExportSource(
	const path& source,
	const path& destination,
	bool replace_existing,
	bool remove_destination_before_copy,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	std::error_code error;
	if (!fs::exists(source, error)) {
		AppendOutputLine(state, "Missing export source: " + source.string());
		return false;
	}

	if (replace_existing &&
		remove_destination_before_copy &&
		fs::exists(destination, error)) {
		fs::remove_all(destination, error);
		if (error) {
			AppendOutputLine(
				state,
				"Failed to replace export destination: " +
					destination.string() + " | " + error.message()
			);
			return false;
		}
	}

	const std::uintmax_t total_files{
		CountExportFiles(source, state)
	};
	std::uintmax_t copied_files{};

	auto update_progress = [&]() {
		state->progress.store(
			std::clamp(
				progress_base +
					progress_scale *
						(static_cast<float>(copied_files) /
						 static_cast<float>(total_files)),
				0.0f,
				1.0f
			),
			std::memory_order_relaxed
		);
	};

	if (fs::is_regular_file(source, error)) {
		if (IsEditorOnlyExportPath(source)) {
			state->progress.store(
				progress_base + progress_scale,
				std::memory_order_relaxed
			);
			return true;
		}

		fs::create_directories(destination.parent_path(), error);
		if (error) {
			return false;
		}

		fs::copy_file(
			source,
			destination,
			replace_existing
				? fs::copy_options::overwrite_existing
				: fs::copy_options::none,
			error
		);
		if (error) {
			AppendOutputLine(
				state,
				"Failed to copy: " + source.string() +
					" | " + error.message()
			);
			return false;
		}

		++copied_files;
		update_progress();
		return true;
	}

	if (!fs::is_directory(source, error)) {
		AppendOutputLine(
			state,
			"Unsupported export source: " + source.string()
		);
		return false;
	}

	fs::create_directories(destination, error);
	if (error) {
		return false;
	}

	for (
		fs::recursive_directory_iterator it{ source, error }, end;
		it != end && !error;
		it.increment(error)
	) {
		if (IsCancelled(state)) {
			return false;
		}

		const path source_path{ it->path() };
		if (IsEditorOnlyExportPath(source_path)) {
			if (it->is_directory(error)) {
				it.disable_recursion_pending();
			}
			error.clear();
			continue;
		}

		const path relative{
			source_path.lexically_relative(source)
		};
		const path destination_path{
			destination / relative
		};

		if (it->is_directory(error)) {
			fs::create_directories(
				destination_path,
				error
			);
		} else if (it->is_regular_file(error)) {
			fs::create_directories(
				destination_path.parent_path(),
				error
			);
			if (!error) {
				fs::copy_file(
					source_path,
					destination_path,
					replace_existing
						? fs::copy_options::overwrite_existing
						: fs::copy_options::none,
					error
				);
			}

			if (!error) {
				++copied_files;
				update_progress();
			}
		}

		if (error) {
			AppendOutputLine(
				state,
				"Failed while exporting: " +
					source_path.string() + " | " +
					error.message()
			);
			return false;
		}
	}

	if (!error) {
		state->progress.store(
			progress_base + progress_scale,
			std::memory_order_relaxed
		);
	}
	return !error;
}

[[nodiscard]] path ExportBuildDirectory(
	const ::ptgn::impl::BuildInfo& info,
	ExportTarget target,
	ExportConfiguration configuration
) {
	return (
		info.binary_directory /
		"ptgn_export" /
		info.target /
		(target == ExportTarget::Desktop
			? "desktop"
			: "web") /
		(configuration == ExportConfiguration::Debug
			? "debug"
			: "release")
	).lexically_normal();
}

[[nodiscard]] bool RunDistributionBuild(
	const ::ptgn::impl::BuildInfo& info,
	ExportTarget target,
	ExportConfiguration configuration,
	bool include_editor,
	const path& build_directory,
	const path& desktop_copy_directory,
	const std::optional<path>& web_project_directory,
	const path& web_project_mount,
	bool preload_app_assets,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	const bool web{ target == ExportTarget::Web };
	const std::string configuration_name{ ConfigurationName(configuration) };

	std::error_code error;
	fs::create_directories(build_directory, error);
	if (error) {
		AppendOutputLine(
			state,
			"Failed to create build cache: " +
				error.message()
		);
		return false;
	}

	std::vector<std::string> configure_arguments{
		"-S",
		info.source_directory.string(),
		"-B",
		build_directory.string(),
		"-DCMAKE_BUILD_TYPE=" + configuration_name,
		std::string{ "-DPTGN_EDITOR=" } +
			(include_editor ? "ON" : "OFF"),
		"-DPTGN_DISTRIBUTION_BUILD=ON",
		"-DPTGN_BUILD_TARGET=" + info.target,
	};

	if (web) {
		configure_arguments.emplace_back("-G");
		configure_arguments.emplace_back("Ninja");
	} else if (!info.generator.empty()) {
		configure_arguments.emplace_back("-G");
		configure_arguments.emplace_back(info.generator);
	}

	if (info.IsExample()) {
		configure_arguments.emplace_back(
			"-DPTGN_EXAMPLES=" +
			info.example_id
		);
	}

	if (!web) {
		configure_arguments.emplace_back(
			"-DPTGN_BUILD_COPY_DIR=" +
			desktop_copy_directory.string()
		);
	} else {
		configure_arguments.emplace_back(
			"-DPTGN_BUILD_COPY_DIR="
		);
		configure_arguments.emplace_back(
			web_project_directory.has_value()
				? "-DPTGN_BUILD_PROJECT_DIR=" +
					web_project_directory->string()
				: "-DPTGN_BUILD_PROJECT_DIR="
		);
		configure_arguments.emplace_back(
			web_project_directory.has_value()
				? "-DPTGN_BUILD_PROJECT_MOUNT=" +
					web_project_mount.generic_string()
				: "-DPTGN_BUILD_PROJECT_MOUNT="
		);
		configure_arguments.emplace_back(
			std::string{
				"-DPTGN_BUILD_PRELOAD_APP_ASSETS="
			} +
			(preload_app_assets ? "ON" : "OFF")
		);
	}

	std::vector<std::string> configure_command_arguments;
	std::string configure_executable{ "cmake" };
	if (web) {
		configure_executable = "emcmake";
		configure_command_arguments.emplace_back(
			"cmake"
		);
	}

	configure_command_arguments.insert(
		configure_command_arguments.end(),
		configure_arguments.begin(),
		configure_arguments.end()
	);

	const float configure_scale{
		progress_scale * 0.15f
	};
	if (!RunLoggedCommand(
			configure_executable,
			configure_command_arguments,
			state,
			progress_base,
			configure_scale
		)) {
		return false;
	}

	const float build_base{
		progress_base + configure_scale
	};
	const float build_scale{
		progress_scale - configure_scale
	};

	const std::vector<std::string> build_arguments{
		"--build",
		build_directory.string(),
		"--config",
		configuration_name,
		"--target",
		info.target,
	};

	if (!RunLoggedCommand(
			"cmake",
			build_arguments,
			state,
			build_base,
			build_scale
		)) {
		return false;
	}

	state->progress.store(
		progress_base + progress_scale,
		std::memory_order_relaxed
	);
	return true;
}

} // namespace

ExportManager::ExportManager() :
	shared_state_{
		std::make_shared<impl::ExportSharedState>()
	} {}

ExportManager::~ExportManager() {
	Cancel();
	if (future_.valid()) {
		future_.wait();
	}
}

bool ExportManager::Export(
	ExportRequest request
) {
	if (IsBusy() ||
		request.output_directory.empty()) {
		return false;
	}

	const auto info{
		::ptgn::impl::GetBuildInfo()
	};
	const path build_directory{
		ExportBuildDirectory(
			info,
			request.target,
			request.configuration
		)
	};
	const impl::ExportTaskKind kind{
		request.target == ExportTarget::Desktop
			? impl::ExportTaskKind::Desktop
			: impl::ExportTaskKind::Web
	};
	const auto state{ shared_state_ };

	ClearOutput();
	state->cancel_requested.store(
		false,
		std::memory_order_relaxed
	);
	state->progress.store(
		0.0f,
		std::memory_order_relaxed
	);
	state->phase.store(
		request.project_directory.has_value()
			? ExportPhase::ProjectFiles
			: ExportPhase::Build,
		std::memory_order_relaxed
	);

	auto future{
		std::async(
			std::launch::async,
			[
				request = std::move(request),
				info,
				build_directory,
				state,
				kind
			]() mutable {
				impl::ExportTaskResult result{
					.kind = kind,
					.output_directory =
						request.output_directory,
				};

				AppendOutputLine(
					state,
					TaskName(kind)
				);
				AppendOutputLine(
					state,
					"Output: " +
						request.output_directory.string()
				);
				AppendOutputLine(
					state,
					"Build cache: " +
						build_directory.string()
				);
				AppendOutputLine(
					state,
					"Configuration: " +
						std::string{ ConfigurationName(request.configuration) }
				);
				AppendOutputLine(
					state,
					std::string{ "Editor: " } +
						(request.include_editor ? "Enabled" : "Disabled")
				);
				AppendOutputLine(state, "");

				const bool web{
					request.target == ExportTarget::Web
				};
				std::optional<path> staged_project_directory;
				const path staging_root{
					build_directory / "runtime_staging"
				};

				if (request.project_directory) {
					state->phase.store(
						ExportPhase::ProjectFiles,
						std::memory_order_relaxed
					);

					std::error_code error;
					fs::remove_all(
						staging_root,
						error
					);
					error.clear();

					const path destination{
						request.project_mount.empty()
							? staging_root
							: staging_root /
								request.project_mount
					};

					AppendOutputLine(
						state,
						"Creating project snapshot..."
					);
					if (!CopyExportSource(
							request.project_directory.value(),
							destination,
							true,
							true,
							state,
							0.0f,
							0.10f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					staged_project_directory = destination;
				}

				if (IsCancelled(state)) {
					result.cancelled = true;
					return result;
				}

				state->phase.store(
					ExportPhase::Build,
					std::memory_order_relaxed
				);

				if (!web) {
					const float build_base{
						request.project_directory
							? 0.10f
							: 0.0f
					};
					const float build_scale{
						request.project_directory
							? 0.75f
							: 0.80f
					};

					if (!RunDistributionBuild(
							info,
							request.target,
							request.configuration,
							request.include_editor,
							build_directory,
							request.output_directory,
							std::nullopt,
							{},
							true,
							state,
							build_base,
							build_scale
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					if (IsCancelled(state)) {
						result.cancelled = true;
						return result;
					}

					state->phase.store(
						ExportPhase::Output,
						std::memory_order_relaxed
					);

					if (staged_project_directory) {
						const path destination{
							request.project_mount.empty()
								? request.output_directory
								: request.output_directory /
									request.project_mount
						};

						AppendOutputLine(
							state,
							"Copying project snapshot to output..."
						);
						if (!CopyExportSource(
								staged_project_directory.value(),
								destination,
								request.replace_existing,
								!request.project_mount.empty(),
								state,
								0.85f,
								0.15f
							)) {
							result.cancelled = IsCancelled(state);
							return result;
						}
					} else if (!request.asset_source_directory.empty()) {
						AppendOutputLine(
							state,
							"Exporting runtime assets: " +
								request.asset_source_directory.string()
						);
						if (!CopyExportSource(
								request.asset_source_directory,
								request.output_directory /
									request.asset_source_directory.filename(),
								request.replace_existing,
								true,
								state,
								0.80f,
								0.20f
							)) {
							result.cancelled = IsCancelled(state);
							return result;
						}
					} else {
						state->progress.store(
							1.0f,
							std::memory_order_relaxed
						);
					}
				} else {
					if (!RunDistributionBuild(
							info,
							request.target,
							request.configuration,
							request.include_editor,
							build_directory,
							{},
							staged_project_directory,
							request.project_mount,
							!request.project_directory.has_value(),
							state,
							request.project_directory
								? 0.10f
								: 0.0f,
							request.project_directory
								? 0.80f
								: 0.90f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					if (IsCancelled(state)) {
						result.cancelled = true;
						return result;
					}

					state->phase.store(
						ExportPhase::Output,
						std::memory_order_relaxed
					);

					const path web_output{
						build_directory / "dist"
					};
					AppendOutputLine(
						state,
						"Copying Web output..."
					);
					if (!CopyExportSource(
							web_output,
							request.output_directory,
							request.replace_existing,
							true,
							state,
							0.90f,
							0.10f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}
				}

				if (IsCancelled(state)) {
					result.cancelled = true;
					return result;
				}

				state->progress.store(
					1.0f,
					std::memory_order_relaxed
				);
				AppendOutputLine(state, "");
				AppendOutputLine(
					state,
					"Export completed successfully."
				);
				result.success = true;
				return result;
			}
		)
	};

	return StartTask(
		kind,
		std::move(future)
	);
}

bool ExportManager::Clean(
	ExportTarget target,
	ExportConfiguration configuration
) {
	if (IsBusy()) {
		return false;
	}

	const path build_directory{
		GetBuildDirectory(
			target,
			configuration
		)
	};
	if (build_directory.empty()) {
		return false;
	}

	const auto state{ shared_state_ };
	ClearOutput();
	state->cancel_requested.store(
		false,
		std::memory_order_relaxed
	);
	state->progress.store(
		0.0f,
		std::memory_order_relaxed
	);
	state->phase.store(
		ExportPhase::Clean,
		std::memory_order_relaxed
	);

	auto future{
		std::async(
			std::launch::async,
			[
				build_directory,
				state
			]() mutable {
				impl::ExportTaskResult result{
					.kind =
						impl::ExportTaskKind::Clean,
					.output_directory =
						build_directory,
				};

				AppendOutputLine(
					state,
					"Cleaning build cache: " +
						build_directory.string()
				);

				std::error_code error;
				if (fs::exists(
						build_directory,
						error
					)) {
					fs::remove_all(
						build_directory,
						error
					);
				}

				if (error) {
					AppendOutputLine(
						state,
						"Clean failed: " +
							error.message()
					);
					return result;
				}

				state->progress.store(
					1.0f,
					std::memory_order_relaxed
				);
				AppendOutputLine(
					state,
					"Clean completed successfully."
				);
				result.success = true;
				return result;
			}
		)
	};

	return StartTask(
		impl::ExportTaskKind::Clean,
		std::move(future)
	);
}

void ExportManager::Cancel() {
	if (!CanCancel()) {
		return;
	}

	if (shared_state_->cancel_requested.exchange(
			true,
			std::memory_order_relaxed
		)) {
		return;
	}
	AppendOutputLine(
		shared_state_,
		"Cancellation requested..."
	);
	TerminateActiveProcess(shared_state_);
}

void ExportManager::OnUpdate() {
	if (!IsBusy() ||
		!future_.valid()) {
		return;
	}

	if (future_.wait_for(0ms) !=
		std::future_status::ready) {
		return;
	}

	impl::ExportTaskResult result{
		future_.get()
	};
	result_directory_ =
		std::move(result.output_directory);

	if (result.cancelled) {
		state_ = ExportTaskState::Cancelled;
		AppendOutputLine(
			shared_state_,
			"Export cancelled."
		);
	} else if (result.success) {
		state_ = ExportTaskState::Succeeded;
	} else {
		state_ = ExportTaskState::Failed;
		AppendOutputLine(
			shared_state_,
			"Export failed."
		);
	}

	shared_state_->phase.store(
		ExportPhase::Idle,
		std::memory_order_relaxed
	);

	if (result.success &&
		result.kind ==
			impl::ExportTaskKind::Desktop) {
		last_desktop_export_directory_ =
			result_directory_;
	} else if (
		result.success &&
		result.kind ==
			impl::ExportTaskKind::Web
	) {
		last_web_export_directory_ =
			result_directory_;
	}
}

void ExportManager::DrawOutputPanel() {
	const auto revision{
		shared_state_->output_revision.load(
			std::memory_order_relaxed
		)
	};

	if (revision !=
		last_rendered_output_revision_) {
		std::scoped_lock lock{
			shared_state_->output_mutex
		};
		rendered_output_ =
			shared_state_->output;
		last_rendered_output_revision_ =
			revision;
	}

	const char* status{ "Idle" };
	switch (state_) {
		case ExportTaskState::Idle:
			status = "Idle";
			break;
		case ExportTaskState::Running:
			status = "Running";
			break;
		case ExportTaskState::Succeeded:
			status = "Succeeded";
			break;
		case ExportTaskState::Failed:
			status = "Failed";
			break;
		case ExportTaskState::Cancelled:
			status = "Cancelled";
			break;
	}

	if (task_kind_ ==
		impl::ExportTaskKind::None) {
		ImGui::TextUnformatted(status);
	} else {
		ImGui::Text(
			"%s - %s",
			TaskName(task_kind_).c_str(),
			status
		);
	}

	ImGui::ProgressBar(
		GetProgress(),
		ImVec2{ -1.0f, 0.0f }
	);

	if (ImGui::Button("Clear Output")) {
		ClearOutput();
		rendered_output_.clear();
		last_rendered_output_revision_ =
			shared_state_->output_revision.load(
				std::memory_order_relaxed
			);
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(
		rendered_output_.empty()
	);
	if (ImGui::Button("Copy All")) {
		ImGui::SetClipboardText(
			rendered_output_.c_str()
		);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(rendered_output_.empty());
	if (ImGui::Button("Jump to Bottom")) {
		jump_to_bottom_requested_ = true;
	}
	ImGui::EndDisabled();

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, 140.0f },
		ImVec2{ FLT_MAX, FLT_MAX }
	);
	if (ImGui::BeginChild(
			"ExportOutputRegion",
			ImVec2{ 0.0f, 300.0f },
			ImGuiChildFlags_Borders |
				ImGuiChildFlags_ResizeY
		)) {
		if (jump_to_bottom_requested_) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::InputTextMultiline(
			"##ExportOutputText",
			rendered_output_.data(),
			rendered_output_.size() + 1,
			ImVec2{ -FLT_MIN, -FLT_MIN },
			ImGuiInputTextFlags_ReadOnly |
				ImGuiInputTextFlags_CallbackAlways,
			ExportOutputTextCallback,
			&jump_to_bottom_requested_
		);
	}
	ImGui::EndChild();
}

bool ExportManager::IsBusy() const {
	return state_ ==
		ExportTaskState::Running;
}

bool ExportManager::CanCancel() const {
	return IsBusy() &&
		task_kind_ !=
			impl::ExportTaskKind::Clean;
}

ExportTaskState ExportManager::GetState() const {
	return state_;
}

ExportPhase ExportManager::GetPhase() const {
	return shared_state_->phase.load(
		std::memory_order_relaxed
	);
}

bool ExportManager::IsExportingProjectFiles() const {
	return IsBusy() &&
		GetPhase() == ExportPhase::ProjectFiles;
}

float ExportManager::GetProgress() const {
	return shared_state_->progress.load(
		std::memory_order_relaxed
	);
}

path ExportManager::GetBuildDirectory(
	ExportTarget target,
	ExportConfiguration configuration
) const {
	return ExportBuildDirectory(
		::ptgn::impl::GetBuildInfo(),
		target,
		configuration
	);
}

std::optional<path>
ExportManager::GetLastExportDirectory(
	ExportTarget target
) const {
	return target == ExportTarget::Desktop
		? last_desktop_export_directory_
		: last_web_export_directory_;
}

bool ExportManager::StartTask(
	impl::ExportTaskKind kind,
	std::future<impl::ExportTaskResult> future
) {
	if (IsBusy() ||
		!future.valid()) {
		return false;
	}

	future_ = std::move(future);
	task_kind_ = kind;
	state_ = ExportTaskState::Running;
	result_directory_.clear();
	return true;
}

void ExportManager::ClearOutput() {
	{
		std::scoped_lock lock{
			shared_state_->output_mutex
		};
		shared_state_->output.clear();
	}

	shared_state_->output_revision.fetch_add(
		1,
		std::memory_order_relaxed
	);
}

} // namespace ptgn::editor

#endif
