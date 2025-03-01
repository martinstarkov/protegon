#include <algorithm>
#include <string>
#include <string_view>

#include "audio/audio.h"
#include "core/game.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/grid.h"
#include "ui/button.h"
#include "utility/time.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

Button CreateButton(
	ecs::Manager& manager, std::string_view content, const ButtonCallback& on_activate,
	const Color& bg_color = color::LightGray
) {
	Button b{ manager };
	b.SetBackgroundColor(bg_color);
	b.SetBackgroundColor(color::Gray, ButtonState::Hover);
	b.SetBackgroundColor(color::DarkGray, ButtonState::Pressed);
	b.SetBordered(true);
	b.SetBorderColor(color::LightGray);
	b.SetBorderWidth(3.0f);
	b.SetText(content, color::Black);
	b.OnActivate(on_activate);
	return b;
}

class AudioExample : public Scene {
public:
	int channel1{ 1 };
	int channel2{ 2 };

	int starting_volume{ 30 };

	Grid<Button> grid{ { 4, 12 } };

	Button* b1{ nullptr };
	Button* b2{ nullptr };
	Button* b3{ nullptr };
	Button* b4{ nullptr };
	Button* b5{ nullptr };
	Button* b6{ nullptr };
	Button* b7{ nullptr };
	Button* b8{ nullptr };
	Button* b9{ nullptr };
	Button* b10{ nullptr };
	Button* b11{ nullptr };
	Button* b12{ nullptr };

	Color music_color{ color::Teal };
	Color sound1_color{ color::Gold };
	Color sound2_color{ color::LightPink };

	void Enter() override {
		game.music.Load("music1", "resources/music1.ogg");
		game.sound.Load("sound1", "resources/sound1.ogg");
		game.music.Load("music2", "resources/music2.ogg");
		game.sound.Load("sound2", "resources/sound2.ogg");

		game.music.SetVolume(starting_volume);
		game.sound.SetVolume("sound1", starting_volume);
		game.sound.SetVolume("sound2", starting_volume);

		b1 = &grid.Set({ 0, 0 }, CreateButton(manager, "Music Volume: ", []() {}, music_color));
		b2 = &grid.Set({ 0, 1 }, CreateButton(manager, "Music Is Playing: ", []() {}, music_color));
		b3 = &grid.Set({ 0, 2 }, CreateButton(manager, "Music Is Paused: ", []() {}, music_color));
		b4 = &grid.Set({ 0, 3 }, CreateButton(manager, "Music Is Fading: ", []() {}, music_color));
		b5 =
			&grid.Set({ 0, 4 }, CreateButton(manager, "Channel 1 Volume: ", []() {}, sound1_color));
		b7 = &grid.Set(
			{ 0, 5 }, CreateButton(
						  manager, "Channel 1 Playing: ", []() {}, sound1_color
					  )
		);
		b9 =
			&grid.Set({ 0, 6 }, CreateButton(manager, "Channel 1 Paused: ", []() {}, sound1_color));
		b11 =
			&grid.Set({ 0, 7 }, CreateButton(manager, "Channel 1 Fading: ", []() {}, sound1_color));
		b6 =
			&grid.Set({ 0, 8 }, CreateButton(manager, "Channel 2 Volume: ", []() {}, sound2_color));
		b8 = &grid.Set(
			{ 0, 9 }, CreateButton(
						  manager, "Channel 2 Playing: ", []() {}, sound2_color
					  )
		);
		b10 = &grid.Set(
			{ 0, 10 }, CreateButton(
						   manager, "Channel 2 Paused: ", []() {}, sound2_color
					   )
		);
		b12 = &grid.Set(
			{ 0, 11 }, CreateButton(
						   manager, "Channel 2 Fading: ", []() {}, sound2_color
					   )
		);

		grid.Set(
			{ 1, 0 }, CreateButton(
						  manager, "Play Music 1", [&]() { game.music.Play("music1"); }, music_color
					  )
		);
		grid.Set(
			{ 1, 1 }, CreateButton(
						  manager, "Play Music 2", [&]() { game.music.Play("music2"); }, music_color
					  )
		);
		grid.Set(
			{ 1, 2 }, CreateButton(
						  manager, "Stop Music", [&]() { game.music.Stop(); }, music_color
					  )
		);
		grid.Set(
			{ 1, 3 }, CreateButton(
						  manager, "Fade In Music 1 (3s)",
						  [&]() { game.music.FadeIn("music1", milliseconds{ 3000 }); }, music_color
					  )
		);
		grid.Set(
			{ 1, 4 }, CreateButton(
						  manager, "Fade In Music 2 (3s)",
						  [&]() { game.music.FadeIn("music2", milliseconds{ 3000 }); }, music_color
					  )
		);
		grid.Set(
			{ 1, 5 }, CreateButton(
						  manager, "Fade Out Music (3s)",
						  [&]() { game.music.FadeOut(milliseconds{ 3000 }); }, music_color
					  )
		);
		grid.Set(
			{ 1, 6 },
			CreateButton(
				manager, "Toggle Music Pause", [&]() { game.music.TogglePause(); }, music_color
			)
		);
		grid.Set(
			{ 1, 7 }, CreateButton(
						  manager, "Toggle Music Mute",
						  [&]() { game.music.ToggleVolume(starting_volume); }, music_color
					  )
		);
		grid.Set(
			{ 1, 8 },
			CreateButton(
				manager, "+ Music Volume",
				[&]() { game.music.SetVolume(std::clamp(game.music.GetVolume() + 5, 0, 128)); },
				music_color
			)
		);
		grid.Set(
			{ 1, 9 },
			CreateButton(
				manager, "- Music Volume",
				[&]() { game.music.SetVolume(std::clamp(game.music.GetVolume() - 5, 0, 128)); },
				music_color
			)
		);

		grid.Set(
			{ 2, 0 }, CreateButton(
						  manager, "Play Channel 1", [&]() { game.sound.Play("sound1", channel1); },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 1 },
			CreateButton(
				manager, "Stop Channel 1", [&]() { game.sound.Stop(channel1); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 2 },
			CreateButton(
				manager, "Fade In Sound 1 (3s)",
				[&]() { game.sound.FadeIn("sound1", milliseconds{ 3000 }, channel1); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 3 },
			CreateButton(
				manager, "Fade Out Channel 1 (3s)",
				[&]() { game.sound.FadeOut(milliseconds{ 3000 }, channel1); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 4 }, CreateButton(
						  manager, "Toggle Channel 1 Pause",
						  [&]() { game.sound.TogglePause(channel1); }, sound1_color
					  )
		);
		grid.Set(
			{ 2, 5 },
			CreateButton(
				manager, "Toggle Sound 1 Mute",
				[&]() { game.sound.ToggleVolume("sound1", starting_volume); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 6 }, CreateButton(
						  manager, "+ Channel 1 Volume",
						  [&]() {
							  game.sound.SetVolume(
								  channel1, std::clamp(game.sound.GetVolume(channel1) + 5, 0, 128)
							  );
						  },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 7 }, CreateButton(
						  manager, "- Channel 1 Volume",
						  [&]() {
							  game.sound.SetVolume(
								  channel1, std::clamp(game.sound.GetVolume(channel1) - 5, 0, 128)
							  );
						  },
						  sound1_color
					  )
		);

		grid.Set(
			{ 3, 0 }, CreateButton(
						  manager, "Play Channel 2", [&]() { game.sound.Play("sound2", channel2); },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 1 },
			CreateButton(
				manager, "Stop Channel 2", [&]() { game.sound.Stop(channel2); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 2 },
			CreateButton(
				manager, "Fade In Sound 2 (3s)",
				[&]() { game.sound.FadeIn("sound2", milliseconds{ 3000 }, channel2); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 3 },
			CreateButton(
				manager, "Fade Out Channel 2 (3s)",
				[&]() { game.sound.FadeOut(milliseconds{ 3000 }, channel2); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 4 }, CreateButton(
						  manager, "Toggle Channel 2 Pause",
						  [&]() { game.sound.TogglePause(channel2); }, sound2_color
					  )
		);
		grid.Set(
			{ 3, 5 },
			CreateButton(
				manager, "Toggle Sound 2 Mute",
				[&]() { game.sound.ToggleVolume("sound2", starting_volume); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 6 }, CreateButton(
						  manager, "+ Channel 2 Volume",
						  [&]() {
							  game.sound.SetVolume(
								  channel2, std::clamp(game.sound.GetVolume(channel2) + 5, 0, 128)
							  );
						  },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 7 }, CreateButton(
						  manager, "- Channel 2 Volume",
						  [&]() {
							  game.sound.SetVolume(
								  channel2, std::clamp(game.sound.GetVolume(channel2) - 5, 0, 128)
							  );
						  },
						  sound2_color
					  )
		);

		V2_int offset{ 6, 6 };
		V2_int size{ (window_size - offset * (grid.GetSize() + V2_int{ 1, 1 })) / grid.GetSize() };

		grid.ForEach([&](auto coord, Button& b) {
			if (b != Button{}) {
				b.SetPosition(coord * size + (coord + V2_int{ 1, 1 }) * offset);
				b.SetRect(size, Origin::TopLeft);
			}
		});
	}

	void Exit() override {
		game.music.Clear();
		game.sound.Clear();
	}

	void Update() override {
		b1->SetTextContent(std::string("Music Volume: ") + std::to_string(game.music.GetVolume()));
		b2->SetTextContent(
			std::string("Music Is Playing: ") + (game.music.IsPlaying() ? "true" : "false")
		);
		b3->SetTextContent(
			std::string("Music Is Paused: ") + (game.music.IsPaused() ? "true" : "false")
		);
		b4->SetTextContent(
			std::string("Music Is Fading: ") + (game.music.IsFading() ? "true" : "false")
		);
		b5->SetTextContent(
			std::string("Channel 1 Volume: ") + std::to_string(game.sound.GetVolume(channel1))
		);
		b6->SetTextContent(
			std::string("Channel 2 Volume: ") + std::to_string(game.sound.GetVolume(channel2))
		);
		b7->SetTextContent(
			std::string("Channel 1 Playing: ") + (game.sound.IsPlaying(channel1) ? "true" : "false")
		);
		b8->SetTextContent(
			std::string("Channel 2 Playing: ") + (game.sound.IsPlaying(channel2) ? "true" : "false")
		);
		b9->SetTextContent(
			std::string("Channel 1 Paused: ") + (game.sound.IsPaused(channel1) ? "true" : "false")
		);
		b10->SetTextContent(
			std::string("Channel 2 Paused: ") + (game.sound.IsPaused(channel2) ? "true" : "false")
		);
		b11->SetTextContent(
			std::string("Channel 1 Fading: ") + (game.sound.IsFading(channel1) ? "true" : "false")
		);
		b12->SetTextContent(
			std::string("Channel 2 Fading: ") + (game.sound.IsFading(channel2) ? "true" : "false")
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AudioExample", window_size);
	game.scene.Enter<AudioExample>("audio");
	return 0;
}