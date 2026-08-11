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
		bool show_relative = false
	) {
		if (request_) {
			return;
		}

		request_ = Request{
			.label = std::move(label),
			.initial = initial,
			.convert = std::move(convert),
			.apply = std::move(apply),
			.reference_world = reference_world,
			.show_relative = show_relative,
		};
	}

	void Cancel() {
		request_.reset();
	}

	[[nodiscard]] bool IsActive() const {
		return request_.has_value();
	}

	[[nodiscard]] std::string_view Label() const {
		return request_ ? std::string_view{ request_->label } : std::string_view{};
	}

	[[nodiscard]] std::optional<V2_float> Initial() const {
		return request_ ? std::optional<V2_float>{ request_->initial } : std::nullopt;
	}

	[[nodiscard]] std::optional<V2_float> ReferenceWorld() const {
		return request_ ? request_->reference_world : std::nullopt;
	}

	[[nodiscard]] std::optional<PreviewData> Preview(V2_float world_position) const {
		if (!request_) {
			return std::nullopt;
		}

		V2_float absolute{ world_position };
		RoundPosition(absolute);

		auto converted{
			request_->convert
				? request_->convert(world_position)
				: std::optional<V2_float>{ world_position }
		};

		if (!converted) {
			return std::nullopt;
		}

		RoundPosition(*converted);

		return PreviewData{
			.absolute = absolute,
			.relative = request_->show_relative
				? std::optional<V2_float>{ *converted }
				: std::nullopt,
			.value = *converted,
			.delta = *converted - request_->initial,
		};
	}

	[[nodiscard]] bool Submit(UndoStack& undo, V2_float world_position) {
		if (!request_) {
			return false;
		}

		auto converted{
			request_->convert
				? request_->convert(world_position)
				: std::optional<V2_float>{ world_position }
		};

		if (!converted) {
			return false;
		}

		RoundPosition(*converted);

		Request request{ std::move(*request_) };
		request_.reset();

		request.apply(*converted);

		undo.PushApplied(
			std::move(request.label),
			[apply = request.apply, before = request.initial]() mutable {
				apply(before);
			},
			[apply = std::move(request.apply), after = *converted]() mutable {
				apply(after);
			}
		);

		return true;
	}

private:
	struct Request {
		std::string label;
		V2_float initial;
		Convert convert;
		Apply apply;
		std::optional<V2_float> reference_world;
		bool show_relative{ false };
	};

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
