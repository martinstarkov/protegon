#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "core/manager.h"
#include "event/key.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "vfx/light.h"

struct TestMouseLight : public Test {
	Texture test{ "resources/sprites/test1.jpg" };

	void Shutdown() override {
		game.light.Reset();
		game.draw.SetClearColor(color::White);
	}

	void Init() override {
		game.draw.SetClearColor(color::Transparent);

		game.light.Reset();
		game.light.Load(0, Light{ V2_float{ 0, 0 }, color::Cyan, 30.0f });
	}

	void Update() override {
		if (game.input.KeyDown(Key::B)) {
			game.light.SetBlur(!game.light.GetBlur());
		}

		game.light.Get(0).SetPosition(game.input.GetMousePosition());
	}

	void Draw() override {
		game.draw.Rect({ 100, 100 }, { 100, 100 }, color::Red, Origin::TopLeft);
		game.draw.Texture(test, game.window.GetSize() / 2, test.GetSize());

		game.light.Draw();
	}
};

struct TestRotatingLights : public Test {
	Texture test{ "resources/sprites/test1.jpg" };

	const float speed{ 300.0f };

	void Shutdown() override {
		game.light.Reset();
		game.draw.SetClearColor(color::White);
	}

	std::unordered_map<LightManager::InternalKey, float> radii;

	void Init() override {
		game.draw.SetClearColor(color::Transparent);

		game.light.Reset();
		game.light.Load(0, Light{ V2_float{ 0, 0 }, color::White });
		game.light.Load(1, Light{ V2_float{ 0, 0 }, color::Green });
		game.light.Load(2, Light{ V2_float{ 0, 0 }, color::Blue });
		game.light.Load(3, Light{ V2_float{ 0, 0 }, color::Magenta });
		game.light.Load(4, Light{ V2_float{ 0, 0 }, color::Yellow });
		game.light.Load(5, Light{ V2_float{ 0, 0 }, color::Cyan });
		game.light.Load(6, Light{ V2_float{ 0, 0 }, color::Red });

		radii.clear();
		radii.reserve(game.light.Size());
		float i{ 0.0f };
		game.light.ForEachKey([&](auto key) {
			radii.emplace(key, i * 50.0f);
			i++;
		});
	}

	void Update() override {
		if (game.input.KeyDown(Key::B)) {
			game.light.SetBlur(!game.light.GetBlur());
		}

		auto t		   = game.time();
		auto frequency = 0.001f;
		V2_float c	   = game.window.GetSize() / 2.0f;

		game.light.ForEachKeyValue([&](auto key, auto& light) {
			auto it = radii.find(key);
			PTGN_ASSERT(it != radii.end());
			float r{ it->second };
			light.SetPosition(V2_float{ r * std::sin(frequency * t) + c.x,
										r * std::cos(frequency * t) + c.y });
		});
	}

	void Draw() override {
		game.draw.Rect({ 100, 100 }, { 100, 100 }, color::Blue, Origin::TopLeft);
		game.draw.Texture(test, game.window.GetSize() / 2, test.GetSize());

		game.light.Draw();
	}
};

void TestLighting() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestMouseLight());
	tests.emplace_back(new TestRotatingLights());

	AddTests(tests);
}