#include "runtime/graphics/particle.h"

#include <functional>
#include <optional>
#include <string_view>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "ecs/ecs.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/particle_presets.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class ParticleScene : public Scene {
public:
	ParticleEmitter p;

	Grid<Button> grid{ { 2, 4 } };

	ParticlePreset current_effect{ ParticlePreset::Smoke1 };

	Button CreateParticleButton(std::string_view content, const std::function<void()>& on_press) {
		Button b{ CreateButton(*this) };
		b.SetBackgroundColor(color::Gold)
			.SetBackgroundColor(color::Red, ButtonState::Hover)
			.SetBackgroundColor(color::DarkRed, ButtonState::Press)
			.SetBorderColor(color::LightGray)
			.SetBorderWidth(3.0f)
			.SetText(content, color::Black)
			.OnPress(on_press);

		return b;
	}

	void RecreateMainEmitter(bool start, std::optional<V2_float> position) {
		p.Reset();
		p.Destroy();
		p = CreateParticleEmitter(
			*this, position.value_or(V2_float{}), GetParticleConfig(current_effect)
		);

		if (start) {
			p.Start();
		}
	}

	void SelectEffect(ParticlePreset preset, std::optional<V2_float> position = std::nullopt) {
		current_effect = preset;
		RecreateMainEmitter(true, position);
	}

	void SetParticleButton(
		V2_int grid_index, std::string_view name, ParticlePreset effect,
		std::optional<V2_float> position = std::nullopt
	) {
		grid.Set(grid_index, CreateParticleButton(name, [this, effect, position]() {
					 SelectEffect(effect, position);
				 }));
	}

	void OnEnter() override {
		p = CreateParticleEmitter(*this, {}, GetParticleConfig(current_effect));
		p.Start();

		V2_float weather_offset{ 0.0f, static_cast<float>(-ctx().renderer.GetGameSize().y) / 2.0f };

		SetParticleButton({ 0, 0 }, "Smoke", ParticlePreset::Smoke1);
		SetParticleButton({ 0, 1 }, "Fire", ParticlePreset::Fire1);
		SetParticleButton({ 0, 2 }, "Explosion", ParticlePreset::FireExplosion1);
		SetParticleButton({ 1, 0 }, "Rain", ParticlePreset::Rain1, weather_offset);
		SetParticleButton({ 1, 1 }, "Snow", ParticlePreset::Snow1, weather_offset);

		grid.Set({ 1, 2 }, CreateParticleButton("Toggle Emission", [this]() { p.Toggle(); }));

		grid.Set({ 1, 3 }, CreateParticleButton("Toggle Gravity", [this]() {
					 auto& emitter = p.Get<impl::ParticleEmitterComponent>();

					 if (constexpr V2_float toggled_gravity{ 0.0f, 300.0f };
						 emitter.config.start_gravity.has_value() &&
						 *emitter.config.start_gravity == toggled_gravity) {
						 emitter.config.start_gravity = std::nullopt;
					 } else {
						 emitter.config.start_gravity = toggled_gravity;
					 }

					 // Apply to existing particles
					 for (auto [e, particle] : emitter.manager.EntitiesWith<Particle>()) {
						 particle.gravity = emitter.config.start_gravity.value_or(V2_float{});
					 }
				 }));

		grid.ForEach([this](auto coord, Button& b) {
			constexpr V2_int offset{ 6, 6 };
			constexpr V2_int size{ 150, 40 };

			if (!b) {
				return;
			}
			SetPosition(
				b, -ctx().renderer.GetGameSize() * 0.5f + coord * size +
					   (coord + V2_int{ 1, 1 }) * offset
			);
			b.SetShape(size);
			SetDrawOrigin(b, Origin::TopLeft);
		});
	}
};

int main(int, char**) {
	Application app{ "ParticleScene" };
	app.StartWith<ParticleScene>();
}