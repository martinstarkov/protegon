#pragma once

#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "commands/undo_stack.h"
#include "core/math/vector2.h"

namespace ptgn::editor {

class PositionPicker {
public:
	using Convert = std::function<std::optional<V2_float>(V2_float)>;
	using Apply = std::function<void(V2_float)>;
	using Finish = std::function<void()>;

	struct PreviewData {
		V2_float absolute;
		std::optional<V2_float> relative;
		V2_float value;
		V2_float delta;
	};

	void Begin(
		std::string label,
		V2_float initial,
		Convert convert,
		Apply apply,
		std::optional<V2_float> reference_world = std::nullopt,
		bool show_relative = false,
		Finish finish = {}
	) {
		if (request_) {
			return;
		}

		request_ = Request{
			.label = std::move(label),
			.initial = initial,
			.current = initial,
			.convert = std::move(convert),
			.apply = std::move(apply),
			.finish = std::move(finish),
			.reference_world = reference_world,
			.show_relative = show_relative,
		};
	}

	void Cancel() {
		if (!request_) {
			return;
		}

		Request request{ std::move(*request_) };
		request_.reset();

		// The value is changed live while dragging, so cancellation must
		// restore the value from before the pick began.
		if (request.dragging && request.current != request.initial) {
			request.apply(request.initial);
		}

		if (request.finish) {
			request.finish();
		}
	}

	[[nodiscard]] bool IsActive() const {
		return request_.has_value();
	}

	[[nodiscard]] bool IsDragging() const {
		return request_ && request_->dragging;
	}

	[[nodiscard]] std::string_view Label() const {
		return request_
			? std::string_view{ request_->label }
			: std::string_view{};
	}

	[[nodiscard]] std::optional<V2_float> Initial() const {
		return request_
			? std::optional<V2_float>{ request_->initial }
			: std::nullopt;
	}

	[[nodiscard]] std::optional<V2_float> ReferenceWorld() const {
		return request_
			? request_->reference_world
			: std::nullopt;
	}

	[[nodiscard]] std::optional<PreviewData> Preview(V2_float world_position) const {
		if (!request_) {
			return std::nullopt;
		}

		V2_float absolute{ world_position };
		RoundPosition(absolute);

		auto converted{
			ConvertPosition(*request_, world_position)
		};

		if (!converted) {
			return std::nullopt;
		}

		return PreviewData{
			.absolute = absolute,
			.relative = request_->show_relative
				? std::optional<V2_float>{ *converted }
				: std::nullopt,
			.value = *converted,
			.delta = *converted - request_->initial,
		};
	}

	/// @brief Starts the live pick interaction.
	/// The picked value is applied immediately.
	bool BeginDrag(V2_float world_position) {
		if (!request_ || request_->dragging) {
			return false;
		}

		auto converted{
			ConvertPosition(*request_, world_position)
		};

		if (!converted) {
			return false;
		}

		request_->dragging = true;

		ApplyCurrent(*converted);

		return true;
	}

	/// @brief Updates the picked value while the left mouse button remains held.
	bool UpdateDrag(V2_float world_position) {
		if (!request_ || !request_->dragging) {
			return false;
		}

		auto converted{
			ConvertPosition(*request_, world_position)
		};

		if (!converted) {
			return false;
		}

		ApplyCurrent(*converted);

		return true;
	}

	/// @brief Completes the active drag and records one undo action.
	bool Complete(UndoStack& undo) {
		if (!request_ || !request_->dragging) {
			return false;
		}

		Request request{ std::move(*request_) };
		request_.reset();

		if (request.current != request.initial) {
			undo.PushApplied(
				std::move(request.label),
				[
					apply = request.apply,
					before = request.initial
				]() mutable {
					apply(before);
				},
				[
					apply = request.apply,
					after = request.current
				]() mutable {
					apply(after);
				}
			);
		}

		if (request.finish) {
			request.finish();
		}

		return true;
	}

private:
	struct Request {
		std::string label;

		V2_float initial;
		V2_float current;

		Convert convert;
		Apply apply;
		Finish finish;

		std::optional<V2_float> reference_world;

		bool show_relative{ false };
		bool dragging{ false };
	};

	[[nodiscard]] static std::optional<V2_float> ConvertPosition(
		const Request& request,
		V2_float world_position
	) {
		auto converted{
			request.convert
				? request.convert(world_position)
				: std::optional<V2_float>{ world_position }
		};

		if (!converted) {
			return std::nullopt;
		}

		RoundPosition(*converted);

		return converted;
	}

	void ApplyCurrent(V2_float value) {
		PTGN_ASSERT(request_);

		if (request_->current == value) {
			return;
		}

		request_->current = value;
		request_->apply(value);
	}

	static void RoundPosition(V2_float& position) {
		position.x = std::round(position.x);
		position.y = std::round(position.y);

		if (position.x == 0.0f) {
			position.x = 0.0f;
		}

		if (position.y == 0.0f) {
			position.y = 0.0f;
		}
	}

	std::optional<Request> request_;
};

} // namespace ptgn::editor