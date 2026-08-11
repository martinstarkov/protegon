#include "editor/build_manager.h"

#if !defined(__EMSCRIPTEN__)

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
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

using TaskKind = build_detail::TaskKind;
using TaskResult = build_detail::TaskResult;
using SharedState = build_detail::SharedState;

struct CopySource {
	path source;
	path destination;
};

[[nodiscard]] std::string TaskName(build_detail::TaskKind kind) {
	switch (kind) {
		case build_detail::TaskKind::BuildGame: return "Game build";
		case build_detail::TaskKind::BuildWeb: return "Web build";
		case build_detail::TaskKind::ExportGame: return "Game export";
		case build_detail::TaskKind::ExportWeb: return "Web export";
		case build_detail::TaskKind::Clean: return "Clean";
		case build_detail::TaskKind::None: break;
	}
	return "Task";
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
	const std::shared_ptr<build_detail::SharedState>& state,
	std::string_view value
) {
	{
		std::scoped_lock lock{ state->output_mutex };
		state->output.append(value);
	}
	state->output_revision.fetch_add(1, std::memory_order_relaxed);
}

void AppendOutputLine(
	const std::shared_ptr<build_detail::SharedState>& state,
	std::string_view value
) {
	AppendOutput(state, value);
	AppendOutput(state, "\n");
}

void UpdateBuildProgressFromText(
	const std::shared_ptr<build_detail::SharedState>& state,
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
	const std::shared_ptr<build_detail::SharedState>& state,
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

void TerminateActiveProcess(const std::shared_ptr<build_detail::SharedState>& state) {
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
	const std::shared_ptr<build_detail::SharedState>& state,
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

void TerminateActiveProcess(const std::shared_ptr<build_detail::SharedState>& state) {
	std::scoped_lock lock{ state->process_mutex };
	if (state->active_process == 0) {
		return;
	}

	const auto pid{ static_cast<pid_t>(state->active_process) };
	kill(-pid, SIGTERM);
	kill(-pid, SIGKILL);
}

#endif

[[nodiscard]] bool IsCancelled(const std::shared_ptr<build_detail::SharedState>& state) {
	return state->cancel_requested.load(std::memory_order_relaxed);
}

[[nodiscard]] bool RunLoggedCommand(
	std::string_view executable,
	const std::vector<std::string>& arguments,
	const std::shared_ptr<build_detail::SharedState>& state,
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

[[nodiscard]] std::optional<path> RelativeToRuntimeRoot(
	const path& source,
	const path& runtime_root
) {
	if (source.empty()) {
		return std::nullopt;
	}

	const path normalized_source{ source.lexically_normal() };
	const path normalized_root{ runtime_root.lexically_normal() };
	if (normalized_root.empty()) {
		return source.filename();
	}

	const path relative{ normalized_source.lexically_relative(normalized_root) };
	if (relative.empty() || relative == ".") {
		return path{};
	}
	for (const auto& component : relative) {
		if (component == "..") {
			return source.filename();
		}
	}
	return relative;
}

[[nodiscard]] bool IsEditorOnlyExportPath(const path& source) {
	return source.extension() == ".ptgnlocal";
}

void AddCopySource(
	std::vector<CopySource>& sources,
	const path& source,
	const path& runtime_root,
	const path& output_directory
) {
	if (source.empty()) {
		return;
	}

	const path normalized_source{ source.lexically_normal() };
	const auto duplicate{ std::ranges::find_if(
		sources,
		[&normalized_source](const CopySource& item) {
			return item.source.lexically_normal() == normalized_source;
		}
	) };
	if (duplicate != sources.end()) {
		return;
	}

	const auto relative{ RelativeToRuntimeRoot(source, runtime_root) };
	const path destination{
		relative.has_value() && !relative->empty()
			? output_directory / relative.value()
			: output_directory / source.filename()
	};

	sources.emplace_back(CopySource{
		.source = source,
		.destination = destination,
	});
}

[[nodiscard]] std::uintmax_t CountExportFiles(
	const std::vector<CopySource>& sources,
	const std::shared_ptr<build_detail::SharedState>& state
) {
	std::uintmax_t count{};
	std::error_code error;
	for (const auto& item : sources) {
		if (IsCancelled(state)) {
			break;
		}
		if (fs::is_regular_file(item.source, error)) {
			if (!IsEditorOnlyExportPath(item.source)) {
				++count;
			}
			error.clear();
			continue;
		}
		if (!fs::is_directory(item.source, error)) {
			error.clear();
			continue;
		}
		for (fs::recursive_directory_iterator it{ item.source, error }, end; it != end && !error; it.increment(error)) {
			if (IsCancelled(state)) {
				break;
			}
			if (it->is_regular_file(error) && !IsEditorOnlyExportPath(it->path())) {
				++count;
			}
			error.clear();
		}
		error.clear();
	}
	return std::max<std::uintmax_t>(count, 1);
}

[[nodiscard]] bool CopyExportSource(
	const CopySource& item,
	bool replace_existing,
	std::uintmax_t total_files,
	std::uintmax_t& copied_files,
	const std::shared_ptr<build_detail::SharedState>& state
) {
	std::error_code error;
	if (!fs::exists(item.source, error)) {
		AppendOutputLine(state, "Missing export source: " + item.source.string());
		return false;
	}

	if (replace_existing && fs::exists(item.destination, error)) {
		fs::remove_all(item.destination, error);
		if (error) {
			AppendOutputLine(
				state,
				"Failed to replace export destination: " + item.destination.string() + " | " + error.message()
			);
			return false;
		}
	}

	if (fs::is_regular_file(item.source, error)) {
		if (IsEditorOnlyExportPath(item.source)) {
			return true;
		}
		fs::create_directories(item.destination.parent_path(), error);
		if (error) {
			return false;
		}
		fs::copy_file(
			item.source,
			item.destination,
			replace_existing ? fs::copy_options::overwrite_existing : fs::copy_options::none,
			error
		);
		if (error) {
			AppendOutputLine(state, "Failed to copy: " + item.source.string() + " | " + error.message());
			return false;
		}
		++copied_files;
		state->progress.store(
			static_cast<float>(copied_files) / static_cast<float>(total_files),
			std::memory_order_relaxed
		);
		return true;
	}

	if (!fs::is_directory(item.source, error)) {
		AppendOutputLine(state, "Unsupported export source: " + item.source.string());
		return false;
	}

	fs::create_directories(item.destination, error);
	if (error) {
		return false;
	}

	for (fs::recursive_directory_iterator it{ item.source, error }, end; it != end && !error; it.increment(error)) {
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

		const path relative{ source_path.lexically_relative(item.source) };
		const path destination{ item.destination / relative };
		if (it->is_directory(error)) {
			fs::create_directories(destination, error);
		} else if (it->is_regular_file(error)) {
			fs::create_directories(destination.parent_path(), error);
			if (!error) {
				fs::copy_file(
					source_path,
					destination,
					replace_existing ? fs::copy_options::overwrite_existing : fs::copy_options::none,
					error
				);
			}
			if (!error) {
				++copied_files;
				state->progress.store(
					static_cast<float>(copied_files) / static_cast<float>(total_files),
					std::memory_order_relaxed
				);
			}
		}

		if (error) {
			AppendOutputLine(state, "Failed while exporting: " + source_path.string() + " | " + error.message());
			return false;
		}
	}
	return !error;
}

} // namespace

BuildManager::BuildManager() : shared_state_{ std::make_shared<SharedState>() } {}

BuildManager::~BuildManager() {
	Cancel();
	if (future_.valid()) {
		future_.wait();
	}
}

bool BuildManager::Build(BuildRequest request) {
	if (IsBusy() || request.build_directory.empty()) {
		return false;
	}

	const impl::BuildInfo info{ impl::GetBuildInfo() };
	const TaskKind kind{
		request.target == BuildTarget::Game
			? TaskKind::BuildGame
			: TaskKind::BuildWeb
	};
	const auto state{ shared_state_ };

	ClearOutput();
	state->cancel_requested.store(false, std::memory_order_relaxed);
	state->progress.store(0.0f, std::memory_order_relaxed);
	output_window_open_ = true;

	auto future{ std::async(
		std::launch::async,
		[request = std::move(request), info, state, kind]() mutable {
			TaskResult result{
				.kind = kind,
				.output_directory = kind == TaskKind::BuildWeb
					? request.build_directory / "dist"
					: request.build_directory,
			};

			AppendOutputLine(state, TaskName(kind));
			AppendOutputLine(state, "Build directory: " + request.build_directory.string());
			if (kind == TaskKind::BuildGame && !request.executable_output_directory.empty()) {
				AppendOutputLine(state, "Executable output: " + request.executable_output_directory.string());
			}
			AppendOutputLine(state, "");

			std::error_code error;
			fs::create_directories(request.build_directory, error);
			if (error) {
				AppendOutputLine(state, "Failed to create build directory: " + error.message());
				return result;
			}
			state->progress.store(0.03f, std::memory_order_relaxed);

			std::vector<std::string> configure_arguments{
				"-S",
				info.source_directory.string(),
				"-B",
				request.build_directory.string(),
				"-DCMAKE_BUILD_TYPE=Release",
				"-DPTGN_EDITOR=OFF",
				"-DPTGN_DISTRIBUTION_BUILD=ON",
				"-DPTGN_BUILD_TARGET=" + info.target,
			};

			if (kind == TaskKind::BuildWeb) {
				configure_arguments.emplace_back("-G");
				configure_arguments.emplace_back("Ninja");
			} else if (!info.generator.empty()) {
				configure_arguments.emplace_back("-G");
				configure_arguments.emplace_back(info.generator);
			}

			if (info.IsExample()) {
				configure_arguments.emplace_back("-DPTGN_EXAMPLES=" + info.example_id);
			}

			if (kind == TaskKind::BuildGame) {
				const bool copy_executable{
					!request.executable_output_directory.empty() &&
					request.executable_output_directory.lexically_normal() !=
						request.build_directory.lexically_normal()
				};
				configure_arguments.emplace_back(
					copy_executable
						? "-DPTGN_BUILD_COPY_DIR=" + request.executable_output_directory.string()
						: "-DPTGN_BUILD_COPY_DIR="
				);
			} else {
				configure_arguments.emplace_back("-DPTGN_BUILD_COPY_DIR=");
				configure_arguments.emplace_back(
					request.web_project_directory.has_value()
						? "-DPTGN_BUILD_PROJECT_DIR=" + request.web_project_directory->string()
						: "-DPTGN_BUILD_PROJECT_DIR="
				);
				configure_arguments.emplace_back(
					request.web_project_directory.has_value()
						? "-DPTGN_BUILD_PROJECT_MOUNT=" + request.web_project_mount.generic_string()
						: "-DPTGN_BUILD_PROJECT_MOUNT="
				);
			}

			std::vector<std::string> configure_command_arguments;
			std::string configure_executable{ "cmake" };
			if (kind == TaskKind::BuildWeb) {
				configure_executable = "emcmake";
				configure_command_arguments.emplace_back("cmake");
			}
			configure_command_arguments.insert(
				configure_command_arguments.end(),
				configure_arguments.begin(),
				configure_arguments.end()
			);

			state->progress.store(0.05f, std::memory_order_relaxed);
			if (!RunLoggedCommand(
					configure_executable,
					configure_command_arguments,
					state,
					0.05f,
					0.10f
				)) {
				result.cancelled = IsCancelled(state);
				return result;
			}

			state->progress.store(0.15f, std::memory_order_relaxed);
			const std::vector<std::string> build_arguments{
				"--build",
				request.build_directory.string(),
				"--config",
				"Release",
				"--target",
				info.target,
			};
			if (!RunLoggedCommand("cmake", build_arguments, state, 0.15f, 0.83f)) {
				result.cancelled = IsCancelled(state);
				return result;
			}

			if (IsCancelled(state)) {
				result.cancelled = true;
				return result;
			}

			state->progress.store(1.0f, std::memory_order_relaxed);
			AppendOutputLine(state, "");
			AppendOutputLine(state, "Build completed successfully.");
			result.success = true;
			return result;
		}
	) };

	return StartTask(kind, std::move(future));
}

bool BuildManager::Export(ExportRequest request) {
	if (IsBusy() || request.output_directory.empty()) {
		return false;
	}

	const TaskKind kind{
		request.target == ExportTarget::Game
			? TaskKind::ExportGame
			: TaskKind::ExportWeb
	};
	const auto state{ shared_state_ };

	ClearOutput();
	state->cancel_requested.store(false, std::memory_order_relaxed);
	state->progress.store(0.0f, std::memory_order_relaxed);
	output_window_open_ = true;

	auto future{ std::async(
		std::launch::async,
		[request = std::move(request), state, kind]() mutable {
			TaskResult result{
				.kind = kind,
				.output_directory = request.output_directory,
			};

			AppendOutputLine(state, TaskName(kind));
			AppendOutputLine(state, "Export directory: " + request.output_directory.string());
			if (kind == TaskKind::ExportWeb) {
				AppendOutputLine(
					state,
					"Note: this exports clean project data only. A Web build is still required to package preloaded files into the playable Web output."
				);
			}
			AppendOutputLine(state, "");

			std::error_code error;
			fs::create_directories(request.output_directory, error);
			if (error) {
				AppendOutputLine(state, "Failed to create export directory: " + error.message());
				return result;
			}

			std::vector<CopySource> sources;
			if (request.project_file.has_value()) {
				AddCopySource(
					sources,
					request.project_file.value(),
					request.runtime_root,
					request.output_directory
				);
			}
			if (request.project_asset_directory.has_value()) {
				AddCopySource(
					sources,
					request.project_asset_directory.value(),
					request.runtime_root,
					request.output_directory
				);
			}
			AddCopySource(
				sources,
				request.asset_source_directory,
				request.runtime_root,
				request.output_directory
			);

			if (sources.empty()) {
				AppendOutputLine(state, "There are no project or asset files to export.");
				return result;
			}

			const std::uintmax_t total_files{ CountExportFiles(sources, state) };
			std::uintmax_t copied_files{};
			for (const auto& item : sources) {
				if (IsCancelled(state)) {
					result.cancelled = true;
					return result;
				}
				AppendOutputLine(state, "Exporting: " + item.source.string());
				if (!CopyExportSource(
						item,
						request.replace_existing,
						total_files,
						copied_files,
						state
					)) {
					result.cancelled = IsCancelled(state);
					return result;
				}
			}

			state->progress.store(1.0f, std::memory_order_relaxed);
			AppendOutputLine(state, "");
			AppendOutputLine(state, "Export completed successfully.");
			result.success = true;
			return result;
		}
	) };

	return StartTask(kind, std::move(future));
}

bool BuildManager::Clean(path build_directory) {
	if (IsBusy() || build_directory.empty()) {
		return false;
	}

	const auto state{ shared_state_ };
	ClearOutput();
	state->cancel_requested.store(false, std::memory_order_relaxed);
	state->progress.store(0.0f, std::memory_order_relaxed);
	output_window_open_ = true;

	auto future{ std::async(
		std::launch::async,
		[build_directory = std::move(build_directory), state]() mutable {
			TaskResult result{
				.kind = TaskKind::Clean,
				.output_directory = build_directory,
			};
			AppendOutputLine(state, "Cleaning build directory: " + build_directory.string());

			std::error_code error;
			if (fs::exists(build_directory, error)) {
				fs::remove_all(build_directory, error);
			}
			if (error) {
				AppendOutputLine(state, "Clean failed: " + error.message());
				return result;
			}

			state->progress.store(1.0f, std::memory_order_relaxed);
			AppendOutputLine(state, "Clean completed successfully.");
			result.success = true;
			return result;
		}
	) };

	return StartTask(TaskKind::Clean, std::move(future));
}

void BuildManager::Cancel() {
	if (!CanCancel()) {
		return;
	}
	shared_state_->cancel_requested.store(true, std::memory_order_relaxed);
	AppendOutputLine(shared_state_, "Cancellation requested...");
	TerminateActiveProcess(shared_state_);
}

void BuildManager::OnUpdate() {
	if (!IsBusy() || !future_.valid()) {
		return;
	}
	if (future_.wait_for(0ms) != std::future_status::ready) {
		return;
	}

	TaskResult result{ future_.get() };
	result_directory_ = std::move(result.output_directory);
	if (result.cancelled) {
		state_ = BuildTaskState::Cancelled;
		AppendOutputLine(shared_state_, "Task cancelled.");
	} else if (result.success) {
		state_ = BuildTaskState::Succeeded;
	} else {
		state_ = BuildTaskState::Failed;
		AppendOutputLine(shared_state_, "Task failed.");
	}

	if (result.success && result.kind == TaskKind::ExportGame) {
		last_game_export_directory_ = result_directory_;
	} else if (result.success && result.kind == TaskKind::ExportWeb) {
		last_web_export_directory_ = result_directory_;
	}
}

void BuildManager::OnRender() {
	if (!output_window_open_) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2{ 760.0f, 460.0f }, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Build Output###BuildOutputWindow", &output_window_open_)) {
		ImGui::End();
		return;
	}

	const char* status{ "Idle" };
	switch (state_) {
		case BuildTaskState::Idle: status = "Idle"; break;
		case BuildTaskState::Running: status = "Running"; break;
		case BuildTaskState::Succeeded: status = "Succeeded"; break;
		case BuildTaskState::Failed: status = "Failed"; break;
		case BuildTaskState::Cancelled: status = "Cancelled"; break;
	}

	ImGui::Text("%s - %s", TaskName(task_kind_).c_str(), status);
	ImGui::ProgressBar(GetProgress(), ImVec2{ -1.0f, 0.0f });

	if (CanCancel()) {
		if (ImGui::Button("Cancel")) {
			Cancel();
		}
		ImGui::SameLine();
	}

	if (ImGui::Button("Clear Output")) {
		ClearOutput();
	}

	if (!result_directory_.empty()) {
		ImGui::SameLine();
		ImGui::TextDisabled("Output: %s", result_directory_.string().c_str());
	}

	ImGui::Separator();

	std::string output_snapshot;
	{
		std::scoped_lock lock{ shared_state_->output_mutex };
		output_snapshot = shared_state_->output;
	}
	const auto revision{ shared_state_->output_revision.load(std::memory_order_relaxed) };
	const bool output_changed{ revision != last_rendered_output_revision_ };
	last_rendered_output_revision_ = revision;

	ImGui::BeginChild(
		"BuildOutputText",
		ImVec2{ 0.0f, 0.0f },
		false,
		ImGuiWindowFlags_HorizontalScrollbar
	);
	const bool was_at_bottom{
		ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 24.0f
	};
	ImGui::TextUnformatted(
		output_snapshot.c_str(),
		output_snapshot.c_str() + output_snapshot.size()
	);
	if (output_changed && was_at_bottom) {
		ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
	ImGui::End();
}

void BuildManager::OpenOutputWindow() {
	output_window_open_ = true;
}

bool BuildManager::IsBusy() const {
	return state_ == BuildTaskState::Running;
}

bool BuildManager::CanCancel() const {
	return IsBusy() && task_kind_ != TaskKind::Clean;
}

BuildTaskState BuildManager::GetState() const {
	return state_;
}

float BuildManager::GetProgress() const {
	return shared_state_->progress.load(std::memory_order_relaxed);
}

std::optional<path> BuildManager::GetLastExportDirectory(ExportTarget target) const {
	return target == ExportTarget::Game
		? last_game_export_directory_
		: last_web_export_directory_;
}

bool BuildManager::StartTask(TaskKind kind, std::future<TaskResult> future) {
	if (IsBusy() || !future.valid()) {
		return false;
	}
	future_ = std::move(future);
	task_kind_ = kind;
	state_ = BuildTaskState::Running;
	result_directory_.clear();
	return true;
}

void BuildManager::ClearOutput() {
	{
		std::scoped_lock lock{ shared_state_->output_mutex };
		shared_state_->output.clear();
	}
	shared_state_->output_revision.fetch_add(1, std::memory_order_relaxed);
}

} // namespace ptgn::editor

#endif
