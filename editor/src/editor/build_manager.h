#pragma once

#if !defined(__EMSCRIPTEN__)

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "core/util/file.h"

namespace ptgn::editor {

enum class BuildTarget {
	Game,
	Web,
};

enum class ExportTarget {
	Game,
	Web,
};

enum class BuildTaskState {
	Idle,
	Running,
	Succeeded,
	Failed,
	Cancelled,
};

struct BuildRequest {
	BuildTarget target{ BuildTarget::Game };
	path build_directory;

	// Native builds only. When empty or equal to build_directory, no post-build copy is requested.
	path executable_output_directory;

	// Web builds only. Projectless StartWith<T>() applications leave these empty.
	std::optional<path> web_project_directory;
	path web_project_mount;
};

struct ExportRequest {
	ExportTarget target{ ExportTarget::Game };
	path output_directory;
	path runtime_root;
	path asset_source_directory;
	std::optional<path> project_file;
	std::optional<path> project_asset_directory;
	bool replace_existing{ false };
};

namespace build_detail {

enum class TaskKind {
	None,
	BuildGame,
	BuildWeb,
	ExportGame,
	ExportWeb,
	Clean,
};

struct SharedState {
	std::mutex output_mutex;
	std::string output;
	std::atomic<std::uint64_t> output_revision{ 0 };
	std::atomic<float> progress{ 0.0f };
	std::atomic<bool> cancel_requested{ false };

	std::mutex process_mutex;
	std::intptr_t active_process{ 0 };
	std::intptr_t active_job{ 0 };
};

struct TaskResult {
	bool success{ false };
	bool cancelled{ false };
	TaskKind kind{ TaskKind::None };
	path output_directory;
};

} // namespace build_detail

class BuildManager {
public:
	BuildManager();
	~BuildManager();

	BuildManager(const BuildManager&) = delete;
	BuildManager& operator=(const BuildManager&) = delete;
	BuildManager(BuildManager&&) = delete;
	BuildManager& operator=(BuildManager&&) = delete;

	bool Build(BuildRequest request);
	bool Export(ExportRequest request);
	bool Clean(path build_directory);

	void Cancel();
	void OnUpdate();
	void OnRender();
	void OpenOutputWindow();

	[[nodiscard]] bool IsBusy() const;
	[[nodiscard]] bool CanCancel() const;
	[[nodiscard]] BuildTaskState GetState() const;
	[[nodiscard]] float GetProgress() const;
	[[nodiscard]] std::optional<path> GetLastExportDirectory(ExportTarget target) const;

private:
	bool StartTask(
		build_detail::TaskKind kind,
		std::future<build_detail::TaskResult> future
	);
	void ClearOutput();

	std::shared_ptr<build_detail::SharedState> shared_state_;
	std::future<build_detail::TaskResult> future_;
	BuildTaskState state_{ BuildTaskState::Idle };
	build_detail::TaskKind task_kind_{ build_detail::TaskKind::None };
	path result_directory_;
	std::optional<path> last_game_export_directory_;
	std::optional<path> last_web_export_directory_;
	bool output_window_open_{ false };
	std::uint64_t last_rendered_output_revision_{ 0 };
};

} // namespace ptgn::editor

#endif
