#pragma once

#include "protegon/circle.h"
#include "protegon/game.h"
#include "protegon/line.h"
#include "protegon/polygon.h"
#include "utility/debug.h"

using namespace ptgn;

bool TestDrawing() {
	game.window.SetSize({ 800, 400 });
	game.window.Show();

	game.RepeatUntilQuit([&]() {
		game.renderer.SetClearColor(color::White);
		game.renderer.Clear();

		Point<float> test01{ 30, 10 };
		Point<float> test02{ 10, 10 };

		// TODO: Fix.
		// test01.Draw(color::Black);
		// test02.Draw(color::Black, 6);

		Rectangle<float> test11{
			{20, 20},
			{30, 20}
		};
		Rectangle<float> test12{
			{60, 20},
			{40, 20}
		};
		Rectangle<float> test13{
			{110, 20},
			 { 50, 20}
		};

		test11.Draw(color::Red);
		test12.Draw(color::Red, 4);
		test13.DrawSolid(color::Red);

		RoundedRectangle<float> test21{
			{20, 50},
			{30, 20},
			5
		};
		RoundedRectangle<float> test22{
			{60, 50},
			{40, 20},
			8
		};
		RoundedRectangle<float> test23{
			{110, 50},
			 { 50, 20},
			 10
		};
		RoundedRectangle<float> test24{
			{ 30, 180},
			 {160,	50},
			  10
		};

		test21.Draw(color::Green);
		test22.Draw(color::Green, 5);
		test23.DrawSolid(color::Green);
		test24.Draw(color::Green, 4);

		Triangle test31{
			{V2_int{ 180 + 120 * 0, 60 }, V2_int{ 180 + 50 + 120 * 0, 20 },
			  V2_int{ 180 + 50 + 50 + 120 * 0, 80 }}
		};
		Triangle test32{
			{V2_int{ 180 + 120 * 1, 60 }, V2_int{ 180 + 50 + 120 * 1, 20 },
			  V2_int{ 180 + 50 + 50 + 120 * 1, 80 }}
		};
		Triangle test33{
			{V2_int{ 180 + 120 * 2, 60 }, V2_int{ 180 + 50 + 120 * 2, 20 },
			  V2_int{ 180 + 50 + 50 + 120 * 2, 80 }}
		};

		test31.Draw(color::Purple);
		test32.Draw(color::Purple, 5);
		test33.DrawSolid(color::Purple);

		std::vector<V2_int> star1{
			V2_int{		550,			 60},
			V2_int{650 - 44,			  60},
			V2_int{		650,		 60 - 44},
			V2_int{650 + 44,			  60},
			V2_int{		750,			 60},
			V2_int{750 - 44,	  60 + 44},
			V2_int{750 - 44, 60 + 44 + 44},
			V2_int{		650,		 60 + 44},
			V2_int{550 + 44, 60 + 44 + 44},
			V2_int{550 + 44,	  60 + 44},
		};

		std::vector<V2_int> star2;
		std::vector<V2_int> star3;

		for (const auto& s : star1) {
			star2.push_back({ s.x, s.y + 100 });
		}

		for (const auto& s : star1) {
			star3.push_back({ s.x, s.y + 200 });
		}

		Polygon test41{ star1 };
		Polygon test42{ star2 };
		Polygon test43{ star3 };

		test41.Draw(color::DarkBlue);
		test42.Draw(color::DarkBlue, 5);
		test43.DrawSolid(color::DarkBlue);

		Circle<float> test51{
			{30, 130},
			 15
		};
		Circle<float> test52{
			{100, 130},
			  30
		};
		Circle<float> test53{
			{180, 130},
			  20
		};

		test51.Draw(color::DarkGrey);
		test52.Draw(color::DarkGrey, 5);
		test53.DrawSolid(color::DarkGrey);

		Capsule<float> test61{
			{V2_float{ 240, 130 }, V2_float{ 350, 200 }},
			10
		};
		Capsule<float> test62{
			{V2_float{ 230, 170 }, V2_float{ 340, 250 }},
			20
		};
		Capsule<float> test63{
			{V2_float{ 400, 230 }, V2_float{ 530, 200 }},
			20
		};
		Capsule<float> test64{
			{V2_float{ 350, 130 }, V2_float{ 500, 100 }},
			15
		};
		Capsule<float> test65{
			{V2_float{ 300, 320 }, V2_float{ 150, 250 }},
			15
		};

		test61.Draw(color::Brown);
		test62.Draw(color::Brown, 8);
		test63.Draw(color::Brown, 5);
		test64.DrawSolid(color::Brown);
		test65.Draw(color::Brown, 3);

		Line<float> test71{
			V2_float{370, 160},
			  V2_float{500, 130}
		};
		Line<float> test72{
			V2_float{370, 180},
			  V2_float{500, 150}
		};

		test71.Draw(color::Black);
		test72.Draw(color::Black, 5);

		Arc<float> test81{
			V2_float{40, 300},
			 15, 0, 90
		};
		Arc<float> test82{
			V2_float{40 + 50, 300},
			  10, 180, 360
		};
		Arc<float> test83{
			V2_float{40 + 50 + 50, 300},
			   20, -90, 180
		};

		test81.Draw(color::DarkGreen);
		test82.Draw(color::DarkGreen, 3);
		test83.DrawSolid(color::DarkGreen);

		Ellipse<float> test91{
			{380, 300},
			  { 10,	30}
		};
		Ellipse<float> test92{
			{440, 300},
			  { 40,	15}
		};
		Ellipse<float> test93{
			{510, 300},
			  {	5,  40}
		};

		test91.Draw(color::Silver);
		test92.Draw(color::Silver, 5);
		test93.DrawSolid(color::Silver);

		game.renderer.Present();
	});
	return true;
}

bool TestProperties() {
	// Test Rectangle unions

	Rectangle<int> test11{
		{60, 20},
		{40, 20}
	};
	Rectangle<float> test12{
		{60.0f, 30.0f},
		  {40.0f, 50.0f}
	};
	Rectangle<int> test13{
		{110, 10},
		 { 50, 10}
	};

	PTGN_ASSERT((test11.pos == V2_int{ 60, 20 }));
	PTGN_ASSERT((test12.pos == V2_float{ 60.0f, 30.0f }));
	PTGN_ASSERT((test13.pos == V2_int{ 110, 10 }));

	PTGN_ASSERT((test11.size == V2_int{ 40, 20 }));
	PTGN_ASSERT((test12.size == V2_float{ 40.0f, 50.0f }));
	PTGN_ASSERT((test13.size == V2_int{ 50, 10 }));

	PTGN_ASSERT(test11.pos.x == 60 && test11.pos.y == 20);
	PTGN_ASSERT(test12.pos.x == 60.0f && test12.pos.y == 30.0f);
	PTGN_ASSERT(test13.pos.x == 110 && test13.pos.y == 10);

	PTGN_ASSERT(test11.size.x == 40 && test11.size.y == 20);
	PTGN_ASSERT(test12.size.x == 40.0f && test12.size.y == 50.0f);
	PTGN_ASSERT(test13.size.x == 50 && test13.size.y == 10);

	return true;
}

bool TestShapes() {
	PTGN_INFO("Starting shape tests...");

	TestProperties();
	TestDrawing();

	PTGN_INFO("All shape tests passed!");
	return true;
}