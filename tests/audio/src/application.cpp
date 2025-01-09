#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class AudioExample : public Scene {
public:
	// @return Button and size of text.
	Button CreateButton(std::string_view content, const ButtonCallback& on_activate, const Color& bg_color = color::Silver) {
		Button b;
		b.Set<ButtonProperty::BackgroundColor>(bg_color);
		b.Set<ButtonProperty::Bordered>(true);
		b.Set<ButtonProperty::BorderColor>(color::LightGray);
		b.Set<ButtonProperty::BorderThickness>(3.0f);
		Text text{ content, color::Black };
		b.Set<ButtonProperty::Text>(text);
		b.Set<ButtonProperty::OnActivate>(on_activate);
		return b;
	}

	std::vector<Button> buttons;

	Music music1{ "resources/music1.ogg" };
	Music music2{ "resources/music2.ogg" };
	Sound sound1{ "resources/sound1.ogg" };
	Sound sound2{ "resources/sound2.ogg" };

	int channel1{ 1 };
	int channel2{ 2 };

	V2_float volume_pos;

	int starting_volume{ 30 };

	Grid<Button> grid{ { 4, 12 } };

	Button b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12;

	Color music_color{ color::Teal };
	Color sound1_color{ color::Gold };
	Color sound2_color{ color::LightPink };

	void Init() override {
		game.music.Load("music", "resources/music1.ogg");
		game.sound.Load("sound", "resources/sound1.ogg");

		game.music.SetVolume(starting_volume);
		sound1.SetVolume(starting_volume);
		sound2.SetVolume(starting_volume);

		b1 = grid.Set({ 0, 0 }, CreateButton("Music Volume: ", [](){
		}, music_color));
		b2 = grid.Set({ 0, 1 }, CreateButton("Music Is Playing: ", [](){
		}, music_color));
		b3 = grid.Set({ 0, 2 }, CreateButton("Music Is Paused: ", [](){
		}, music_color));
		b4 = grid.Set({ 0, 3 }, CreateButton("Music Is Fading: ", [](){
		}, music_color));
		b5 = grid.Set({ 0, 4 }, CreateButton("Channel 1 Volume: ", [](){
		}, sound1_color));
		b7 = grid.Set({ 0, 5 }, CreateButton("Channel 1 Playing: ", [](){
		}, sound1_color));
		b9 = grid.Set({ 0, 6 }, CreateButton("Channel 1 Paused: ", [](){
		}, sound1_color));
		b11 = grid.Set({ 0, 7 }, CreateButton("Channel 1 Fading: ", [](){
		}, sound1_color));
		b6 = grid.Set({ 0, 8 }, CreateButton("Channel 2 Volume: ", [](){
		}, sound2_color));
		b8 = grid.Set({ 0, 9 }, CreateButton("Channel 2 Playing: ", [](){
		}, sound2_color));
		b10 = grid.Set({ 0, 10 }, CreateButton("Channel 2 Paused: ", [](){
		}, sound2_color));
		b12 = grid.Set({ 0, 11 }, CreateButton("Channel 2 Fading: ", [](){
		}, sound2_color));

		grid.Set({ 1, 0 }, CreateButton("Play Music 1", [&](){
			music1.Play();
		}, music_color));
		grid.Set({ 1, 1 }, CreateButton("Play Music 2", [&](){
			music2.Play();
		}, music_color));
		grid.Set({ 1, 2 }, CreateButton("Stop Music", [&](){
			game.music.Stop();
		}, music_color));
		grid.Set({ 1, 3 }, CreateButton("Fade In Music 1 (3s)", [&](){
			music1.FadeIn(milliseconds{ 3000 });
		}, music_color));
		grid.Set({ 1, 4 }, CreateButton("Fade In Music 2 (3s)", [&](){
			music2.FadeIn(milliseconds{ 3000 });
		}, music_color));
		grid.Set({ 1, 5 }, CreateButton("Fade Out Music (3s)", [&](){
			game.music.FadeOut(milliseconds{ 3000 });
		}, music_color));
		grid.Set({ 1, 6 }, CreateButton("Toggle Music Pause", [&](){
			game.music.TogglePause();
		}, music_color));
		grid.Set({ 1, 7 }, CreateButton("Toggle Music Mute", [&](){
			game.music.ToggleMute(starting_volume);
		}, music_color));
		grid.Set({ 1, 8 }, CreateButton("+ Music Volume", [&](){
			game.music.SetVolume(std::clamp(game.music.GetVolume() + 5, 0, 128));
		}, music_color));
		grid.Set({ 1, 9 }, CreateButton("- Music Volume", [&](){
			game.music.SetVolume(std::clamp(game.music.GetVolume() - 5, 0, 128));
		}, music_color));

		grid.Set({ 2, 0 }, CreateButton("Play Channel 1", [&](){
			sound1.Play(channel1);
		}, sound1_color));
		grid.Set({ 2, 1 }, CreateButton("Stop Channel 1", [&](){
			game.sound.Stop(channel1);
		}, sound1_color));
		grid.Set({ 2, 2 }, CreateButton("Fade In Sound 1 (3s)", [&](){
			sound1.FadeIn(milliseconds{ 3000 }, channel1);
		}, sound1_color));
		grid.Set({ 2, 3 }, CreateButton("Fade Out Channel 1 (3s)", [&](){
			game.sound.FadeOut(milliseconds{ 3000 }, channel1);
		}, sound1_color));
		grid.Set({ 2, 4 }, CreateButton("Toggle Channel 1 Pause", [&](){
			game.sound.TogglePause(channel1);
		}, sound1_color));
		grid.Set({ 2, 5 }, CreateButton("Toggle Sound 1 Mute", [&](){
			sound1.ToggleMute(starting_volume);
		}, sound1_color));
		grid.Set({ 2, 6 }, CreateButton("+ Channel 1 Volume", [&](){
			game.sound.SetVolume(channel1, std::clamp(game.sound.GetVolume(channel1) + 5, 0, 128));
		}, sound1_color));
		grid.Set({ 2, 7 }, CreateButton("- Channel 1 Volume", [&](){
			game.sound.SetVolume(channel1, std::clamp(game.sound.GetVolume(channel1) - 5, 0, 128));
		}, sound1_color));

		grid.Set({ 3, 0 }, CreateButton("Play Channel 2", [&](){
			sound2.Play(channel2);
		}, sound2_color));
		grid.Set({ 3, 1 }, CreateButton("Stop Channel 2", [&](){
			game.sound.Stop(channel2);
		}, sound2_color));
		grid.Set({ 3, 2 }, CreateButton("Fade In Sound 2 (3s)", [&](){
			sound2.FadeIn(milliseconds{ 3000 }, channel2);
		}, sound2_color));
		grid.Set({ 3, 3 }, CreateButton("Fade Out Channel 2 (3s)", [&](){
			game.sound.FadeOut(milliseconds{ 3000 }, channel2);
		}, sound2_color));
		grid.Set({ 3, 4 }, CreateButton("Toggle Channel 2 Pause", [&](){
			game.sound.TogglePause(channel2);
		}, sound2_color));
		grid.Set({ 3, 5 }, CreateButton("Toggle Sound 2 Mute", [&](){
			sound2.ToggleMute(starting_volume);
		}, sound2_color));
		grid.Set({ 3, 6 }, CreateButton("+ Channel 2 Volume", [&](){
			game.sound.SetVolume(channel2, std::clamp(game.sound.GetVolume(channel2) + 5, 0, 128));
		}, sound2_color));
		grid.Set({ 3, 7 }, CreateButton("- Channel 2 Volume", [&](){
			game.sound.SetVolume(channel2, std::clamp(game.sound.GetVolume(channel2) - 5, 0, 128));
		}, sound2_color));

		V2_int offset{ 6, 6 };
		V2_int size{ (resolution - offset * (grid.GetSize() + V2_int{ 1, 1 })) / grid.GetSize() };

		grid.ForEach([&](auto coord, Button& b) {
			b.SetRect({ coord * size + (coord + V2_int{ 1, 1 }) * offset, size, Origin::TopLeft });
		});
	}

	void Shutdown() override {
		game.music.Reset();
		game.sound.Reset();
	}

	void Update() override {
		b1.Set<ButtonProperty::Text>(Text{ std::string("Music Volume: ") + std::to_string(game.music.GetVolume()) });
		b2.Set<ButtonProperty::Text>(Text{ std::string("Music Is Playing: ") + (game.music.IsPlaying() ? "true" : "false") });
		b3.Set<ButtonProperty::Text>(Text{ std::string("Music Is Paused: ") + (game.music.IsPaused() ? "true" : "false") });
		b4.Set<ButtonProperty::Text>(Text{ std::string("Music Is Fading: ") + (game.music.IsFading() ? "true" : "false") });
		b5.Set<ButtonProperty::Text>(Text{ std::string("Channel 1 Volume: ") + std::to_string(game.sound.GetVolume(channel1)) });
		b6.Set<ButtonProperty::Text>(Text{ std::string("Channel 2 Volume: ") + std::to_string(game.sound.GetVolume(channel2)) });
		b7.Set<ButtonProperty::Text>(Text{ std::string("Channel 1 Playing: ") + (game.sound.IsPlaying(channel1) ? "true" : "false") });
		b8.Set<ButtonProperty::Text>(Text{ std::string("Channel 2 Playing: ") + (game.sound.IsPlaying(channel2) ? "true" : "false") });
		b9.Set<ButtonProperty::Text>(Text{ std::string("Channel 1 Paused: ") + (game.sound.IsPaused(channel1) ? "true" : "false") });
		b10.Set<ButtonProperty::Text>(Text{ std::string("Channel 2 Paused: ") + (game.sound.IsPaused(channel2) ? "true" : "false") });
		b11.Set<ButtonProperty::Text>(Text{ std::string("Channel 1 Fading: ") + (game.sound.IsFading(channel1) ? "true" : "false") });
		b12.Set<ButtonProperty::Text>(Text{ std::string("Channel 2 Fading: ") + (game.sound.IsFading(channel2) ? "true" : "false") });

		grid.ForEachElement([](Button& b){
			b.Draw();
		});
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AudioExample", resolution);
	game.scene.LoadActive<AudioExample>("audio");
	return 0;
}
