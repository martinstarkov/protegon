#include <algorithm>
#include <array>
#include <iterator>
#include <magic_enum/magic_enum.hpp>
#include <string>

#include "app/application.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "platform/window.h"
#include "renderer/renderer_settings.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

namespace {

void IncrementToneMappingOperator(ToneMappingOperator& op, int increment) {
	constexpr auto& values{ magic_enum::enum_values<ToneMappingOperator>() };
	auto it{ std::ranges::find(values, op) };
	PTGN_ASSERT(it != values.end(), "Unknown ToneMappingOperator");
	int index{ static_cast<int>(std::ranges::distance(values.begin(), it)) };
	auto new_index{ Mod(index + increment, static_cast<int>(values.size())) };
	PTGN_ASSERT(new_index >= 0 && new_index < static_cast<int>(values.size()));
	op = values[static_cast<std::size_t>(new_index)];
}

} // namespace

class ToneMappingScene : public Scene {
	static constexpr float kExposureStep{ 0.1f };
	static constexpr float kMinExposure{ 0.0f };

	bool bloom_enabled{ true };
	float exposure{ 1.0f };
	ToneMappingOperator tone_mapping_operator{ ToneMappingOperator::Exposure };

	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, {}, "sprite");
		CreateRect(*this, { 0, -200 }, { 100, 100 }, color::Blue);

		ApplyToneMappingSettings();
		EnableBloom();
		UpdateTitle();
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			IncrementToneMappingOperator(tone_mapping_operator, 1);
			ApplyToneMappingSettings();
			UpdateTitle();
		} else if (ctx().input.MousePressed(Mouse::Right)) {
			IncrementToneMappingOperator(tone_mapping_operator, -1);
			ApplyToneMappingSettings();
			UpdateTitle();
		}

		if (ctx().input.KeyPressed(Key::B)) {
			ToggleBloom();
			UpdateTitle();
		}

		auto scroll{ ctx().input.GetMouseScroll().y };

		if (scroll != 0.0f) {
			exposure += scroll * kExposureStep;
			exposure  = std::max(kMinExposure, exposure);

			ApplyToneMappingSettings();
			UpdateTitle();
		}
	}

	void EnableBloom() {
		ClearScreenEffects(*this);

		AddScreenEffect<Bloom>(*this, Bloom{ .threshold = 0.005f, .soft_knee = 0.01f });

		bloom_enabled = true;
	}

	void DisableBloom() {
		ClearScreenEffects(*this);
		bloom_enabled = false;
	}

	void ToggleBloom() {
		if (bloom_enabled) {
			DisableBloom();
		} else {
			EnableBloom();
		}
	}

	void ApplyToneMappingSettings() {
		ctx().renderer.SetToneMappingOperator(tone_mapping_operator);
		ctx().renderer.SetToneMappingExposure(exposure);
	}

	void UpdateTitle() {
		std::string title{ "ToneMappingScene: Tone Mapping Operator (click): " };
		title += magic_enum::enum_name(tone_mapping_operator);
		title += "; Exposure (scroll): ";
		title += std::to_string(exposure);
		title += "; Bloom (B): ";
		title += bloom_enabled ? "On" : "Off";

		ctx().window.SetTitle(title);
	}
};

int main(int, char**) {
	Application app{ "ToneMappingScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ToneMappingScene>();
}