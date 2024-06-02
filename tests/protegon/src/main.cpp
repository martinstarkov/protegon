#include "tests/test_ecs.h"
#include "test_math.h"
#include "test_vector2.h"
#include "test_rng.h"
#include "test_shader.h"

#include "protegon/protegon.h"

using namespace ptgn;

class Tests : public Scene {
public:
    Tests() {
        TestECS();
        TestMath();
        TestVector2();
        TestRNG();
        TestShader();
        window::SetSize({ 800, 400 });
    }
    void Render() {
        static bool drawing_tests_passed = false;

        if (!drawing_tests_passed) {
            std::cout << "Starting drawing tests..." << std::endl;
        }

        Point<float> test01{ 30, 10 };
        Point<float> test02{ 10, 10 };

        test01.Draw(color::BLACK);
        test02.Draw(color::BLACK, 6);

        Rectangle<float> test11{ { 20, 20 }, { 30, 20 } };
        Rectangle<float> test12{ { 60, 20 }, { 40, 20 } };
        Rectangle<float> test13{ { 110, 20 }, { 50, 20 } };

        test11.Draw(color::RED);
        test12.Draw(color::RED, 4);
        test13.DrawSolid(color::RED);

        RoundedRectangle<float> test21{ { 20, 50 }, { 30, 20 }, 5 };
        RoundedRectangle<float> test22{ { 60, 50 }, { 40, 20 }, 8 };
        RoundedRectangle<float> test23{ { 110, 50 }, { 50, 20 }, 10 };
        RoundedRectangle<float> test24{ { 30, 180 }, { 160, 50 }, 10 };

        test21.Draw(color::GREEN);
        test22.Draw(color::GREEN, 5);
        test23.DrawSolid(color::GREEN);
        test24.Draw(color::GREEN, 4);

        Triangle test31{ { V2_int{ 180 + 120 * 0, 60 }, V2_int{ 180 + 50 + 120 * 0, 20 }, V2_int{ 180 + 50 + 50 + 120 * 0, 80 } } };
        Triangle test32{ { V2_int{ 180 + 120 * 1, 60 }, V2_int{ 180 + 50 + 120 * 1, 20 }, V2_int{ 180 + 50 + 50 + 120 * 1, 80 } } };
        Triangle test33{ { V2_int{ 180 + 120 * 2, 60 }, V2_int{ 180 + 50 + 120 * 2, 20 }, V2_int{ 180 + 50 + 50 + 120 * 2, 80 } } };

        test31.Draw(color::PURPLE);
        test32.Draw(color::PURPLE, 5);
        test33.DrawSolid(color::PURPLE);

        std::vector<V2_int> star1{
            V2_int{ 550, 60 },
            V2_int{ 650 - 44, 60 },
            V2_int{ 650, 60 - 44 },
            V2_int{ 650 + 44, 60 },
            V2_int{ 750, 60 },
            V2_int{ 750 - 44, 60 + 44 },
            V2_int{ 750 - 44, 60 + 44 + 44 },
            V2_int{ 650, 60 + 44 },
            V2_int{ 550 + 44, 60 + 44 + 44 },
            V2_int{ 550 + 44, 60 + 44 },
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

        test41.Draw(color::DARK_BLUE);
        test42.Draw(color::DARK_BLUE, 5);
        test43.DrawSolid(color::DARK_BLUE);

        Circle<float> test51{ { 30, 130 }, 15 };
        Circle<float> test52{ { 100, 130 }, 30 };
        Circle<float> test53{ { 180, 130 }, 20 };

        test51.Draw(color::DARK_GREY);
        test52.Draw(color::DARK_GREY, 5);
        test53.DrawSolid(color::DARK_GREY);

        Capsule<float> test61{ { V2_float{ 240, 130 }, V2_float{ 350, 200 } }, 10 };
        Capsule<float> test62{ { V2_float{ 230, 170 }, V2_float{ 340, 250 } }, 20 };
        Capsule<float> test63{ { V2_float{ 400, 230 }, V2_float{ 530, 200 } }, 20 };
        Capsule<float> test64{ { V2_float{ 350, 130 }, V2_float{ 500, 100 } }, 15 };
        Capsule<float> test65{ { V2_float{ 300, 320 }, V2_float{ 150, 250 } }, 15 };

        test61.Draw(color::BROWN);
        test62.Draw(color::BROWN, 8);
        test63.Draw(color::BROWN, 5);
        test64.DrawSolid(color::BROWN);
        test65.Draw(color::BROWN, 3);

        Line<float> test71{ V2_float{ 370, 160 }, V2_float{ 500, 130 } };
        Line<float> test72{ V2_float{ 370, 180 }, V2_float{ 500, 150 } };

        test71.Draw(color::BLACK);
        test72.Draw(color::BLACK, 5);

        Arc<float> test81{ V2_float{ 40, 300 }, 15, 0, 90 };
        Arc<float> test82{ V2_float{ 40 + 50, 300 }, 10, 180, 360 };
        Arc<float> test83{ V2_float{ 40 + 50 + 50, 300 }, 20, -90, 180 };

        test81.Draw(color::DARK_GREEN);
        test82.Draw(color::DARK_GREEN, 3);
        test83.DrawSolid(color::DARK_GREEN);

        Ellipse<float> test91{ { 380, 300 }, { 10, 30 } };
        Ellipse<float> test92{ { 440, 300 }, { 40, 15 } };
        Ellipse<float> test93{ { 510, 300 }, { 5, 40 } };

        test91.Draw(color::SILVER);
        test92.Draw(color::SILVER, 5);
        test93.DrawSolid(color::SILVER);

        if (!drawing_tests_passed) {
            std::cout << "All drawing tests passed!" << std::endl;
            drawing_tests_passed = true;
        }

    }
    void Update(float dt) final {
        static std::chrono::time_point<std::chrono::steady_clock> oldTime = std::chrono::high_resolution_clock::now();
        static int fps; fps++;

        Render();

        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - oldTime) >= std::chrono::seconds{ 1 }) {
            oldTime = std::chrono::high_resolution_clock::now();
            PrintLine("FPS: ", fps);
            fps = 0;
        }
    }
};

int main(int c, char** v) {
    ptgn::game::Start<Tests>();
    return 0;
}