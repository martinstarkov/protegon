#include "core/log.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ptgn::impl {

namespace {

using Clock = std::chrono::system_clock;

struct ConsoleLine {
	Clock::time_point timestamp;
	std::string text;
};

[[nodiscard]] std::tm LocalTime(std::time_t value) {
	std::tm result{};
#if defined(_WIN32)
	localtime_s(&result, &value);
#else
	localtime_r(&value, &result);
#endif
	return result;
}

[[nodiscard]] std::string FormatTimestamp(Clock::time_point timestamp) {
	const auto time{ Clock::to_time_t(timestamp) };
	const auto local_time{ LocalTime(time) };
	const auto milliseconds{
		std::chrono::duration_cast<std::chrono::milliseconds>(
			timestamp.time_since_epoch()
		).count() % 1000
	};

	std::ostringstream output;
	output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
		   << '.' << std::setfill('0') << std::setw(3) << milliseconds;
	return output.str();
}

class ConsoleCaptureStore {
public:
	void Append(std::string_view value) {
		if (value.empty()) {
			return;
		}

		const auto timestamp{ Clock::now() };
		std::scoped_lock lock{ mutex_ };

		output_.append(value);

		for (char c : value) {
			if (!pending_line_timestamp_.has_value()) {
				pending_line_timestamp_ = timestamp;
			}

			if (c == '\n') {
				lines_.emplace_back(ConsoleLine{
					.timestamp = pending_line_timestamp_.value(),
					.text = std::move(pending_line_),
				});
				pending_line_.clear();
				pending_line_timestamp_.reset();
				continue;
			}

			pending_line_.push_back(c);
		}

		revision_.fetch_add(1, std::memory_order_relaxed);
	}

	[[nodiscard]] std::uint64_t Revision() const {
		return revision_.load(std::memory_order_relaxed);
	}

	[[nodiscard]] ConsoleOutputSnapshot Snapshot() const {
		std::scoped_lock lock{ mutex_ };
		return ConsoleOutputSnapshot{
			.revision = revision_.load(std::memory_order_relaxed),
			.output = output_,
		};
	}

	void Clear() {
		std::scoped_lock lock{ mutex_ };
		output_.clear();
		lines_.clear();
		pending_line_.clear();
		pending_line_timestamp_.reset();
		revision_.fetch_add(1, std::memory_order_relaxed);
	}

	[[nodiscard]] bool Save(const std::filesystem::path& output_path) const {
		std::vector<ConsoleLine> lines;
		std::string pending_line;
		std::optional<Clock::time_point> pending_line_timestamp;

		{
			std::scoped_lock lock{ mutex_ };
			lines = lines_;
			pending_line = pending_line_;
			pending_line_timestamp = pending_line_timestamp_;
		}

		std::error_code error;
		const std::filesystem::path parent{ output_path.parent_path() };
		if (!parent.empty()) {
			std::filesystem::create_directories(parent, error);
			if (error) {
				return false;
			}
		}

		std::ofstream output{ output_path, std::ios::binary | std::ios::trunc };
		if (!output) {
			return false;
		}

		output << "Protegon Console Log\n";
		output << "Saved: " << FormatTimestamp(Clock::now()) << "\n\n";

		for (const auto& line : lines) {
			output << '[' << FormatTimestamp(line.timestamp) << "] "
				   << line.text << '\n';
		}

		if (pending_line_timestamp.has_value()) {
			output << '[' << FormatTimestamp(pending_line_timestamp.value()) << "] "
				   << pending_line;
		}

		return output.good();
	}

private:
	mutable std::mutex mutex_;
	std::string output_;
	std::vector<ConsoleLine> lines_;
	std::string pending_line_;
	std::optional<Clock::time_point> pending_line_timestamp_;
	std::atomic<std::uint64_t> revision_{ 0 };
};

class TeeStreamBuffer final : public std::streambuf {
public:
	TeeStreamBuffer(std::streambuf* destination, ConsoleCaptureStore& capture) :
		destination_{ destination }, capture_{ capture } {}

protected:
	int_type overflow(int_type value) override {
		if (traits_type::eq_int_type(value, traits_type::eof())) {
			return traits_type::not_eof(value);
		}

		const char character{ traits_type::to_char_type(value) };
		const auto result{ destination_->sputc(character) };
		if (traits_type::eq_int_type(result, traits_type::eof())) {
			return traits_type::eof();
		}

		capture_.Append(std::string_view{ &character, 1 });
		return value;
	}

	std::streamsize xsputn(const char* value, std::streamsize count) override {
		const auto written{ destination_->sputn(value, count) };
		if (written > 0) {
			capture_.Append(std::string_view{
				value,
				static_cast<std::size_t>(written),
			});
		}
		return written;
	}

	int sync() override {
		return destination_->pubsync();
	}

private:
	std::streambuf* destination_{ nullptr };
	ConsoleCaptureStore& capture_;
};

class ConsoleCapture {
public:
	ConsoleCapture() :
		cout_buffer_{ std::cout.rdbuf(), store_ },
		cerr_buffer_{ std::cerr.rdbuf(), store_ },
		original_cout_{ std::cout.rdbuf(&cout_buffer_) },
		original_cerr_{ std::cerr.rdbuf(&cerr_buffer_) } {}

	~ConsoleCapture() {
		std::cout.rdbuf(original_cout_);
		std::cerr.rdbuf(original_cerr_);
	}

	ConsoleCapture(const ConsoleCapture&) = delete;
	ConsoleCapture& operator=(const ConsoleCapture&) = delete;

	[[nodiscard]] ConsoleCaptureStore& Store() {
		return store_;
	}

private:
	ConsoleCaptureStore store_;
	TeeStreamBuffer cout_buffer_;
	TeeStreamBuffer cerr_buffer_;
	std::streambuf* original_cout_{ nullptr };
	std::streambuf* original_cerr_{ nullptr };
};

ConsoleCapture console_capture;

std::string Basename(std::string_view path) {
	try {
		return std::filesystem::path(path).filename().string();
	} catch (...) {
		return std::string(path);
	}
}

} // namespace

void DebugPrint(std::string_view prefix, std::string_view message, std::source_location where) {
	const auto file{ Basename(where.file_name()) };
	if (!message.empty()) {
		PrintLine(prefix, file, ':', where.line(), " in ", where.function_name(), ": ", message);
	} else {
		PrintLine(prefix, file, ':', where.line(), " in ", where.function_name());
	}
}

std::uint64_t GetConsoleOutputRevision() {
	return console_capture.Store().Revision();
}

ConsoleOutputSnapshot GetConsoleOutputSnapshot() {
	return console_capture.Store().Snapshot();
}

void ClearConsoleOutput() {
	console_capture.Store().Clear();
}

bool SaveConsoleOutput(const std::filesystem::path& output_path) {
	return console_capture.Store().Save(output_path);
}

} // namespace ptgn::impl
