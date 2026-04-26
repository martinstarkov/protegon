#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

class StyledTextBuilder {
public:
	StyledTextBuilder() = delete;

	explicit StyledTextBuilder(const TextRunStyle& default_style) :
		current_style_{ default_style } {}

	StyledTextBuilder& Text(std::string_view text) {
		TextRun run;
		run.text  = std::string{ text };
		run.style = current_style_;
		styled_.runs.push_back(std::move(run));
		return *this;
	}

	StyledTextBuilder& Color(Color color) {
		current_style_.color = color;
		return *this;
	}

	StyledTextBuilder& Scale(float scale) {
		current_style_.scale = scale;
		return *this;
	}

	StyledTextBuilder& Bold(bool enabled = true) {
		if (enabled) {
			current_style_.flags = current_style_.flags | FontStyle::Bold;
		}
		return *this;
	}

	StyledTextBuilder& Italic(bool enabled = true) {
		if (enabled) {
			current_style_.flags = current_style_.flags | FontStyle::Italic;
		}
		return *this;
	}

	StyledText Build() const {
		return styled_;
	}

private:
	TextRunStyle current_style_;
	StyledText styled_;
};

} // namespace ptgn