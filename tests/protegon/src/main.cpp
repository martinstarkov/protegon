#include "tests/test_ecs.h"
#include "test_math.h"
#include "test_vector2.h"
#include "test_rng.h"

#include "protegon/protegon.h"

using namespace ptgn;

class Tests : public Engine {
    void Create() final {
        TestECS();
        TestMath();
        TestVector2();
        TestRNG();
    }
    void Render() {
        Rectangle<float> test1{ { 0, 0 }, { 400, 400 } };
        Rectangle<float> test2{ { 400, 0 }, { 400, 400 } };
        Rectangle<float> test3{ { 0, 400 }, { 400, 400 } };
        Circle<float> test4{ { 200, 200 }, 100 };
        Circle<float> test5{ { 600, 200 }, 100 };
        Circle<float> test6{ { 200, 600 }, 100 };
        Line<float> test7{ { 200, 200 }, { 300, 300 } };
        Line<float> test8{ { 200, 600 }, { 300, 700 } };
        Point<float> test9{ 500, 500 };
        Point<float> test10{ 550, 550 };
        Point<float> test11{ 600, 600 };
        Capsule<float> test12{ { V2_float{ 600, 600 }, V2_float{ 700, 700 } }, 10 };
        Capsule<float> test13{ { V2_float{ 650, 600 }, V2_float{ 750, 700 } }, 20 };
        test1.Draw(color::RED);
        test2.DrawSolid(color::BLUE);
        test3.Draw(color::GREEN, 40);
        test4.Draw(color::BLUE);
        test5.DrawSolid(color::GREEN);
        test6.Draw(color::RED, 40);
        test7.Draw(color::ORANGE);
        test8.Draw(color::CYAN, 40);
        test9.Draw(color::BLACK);
        test10.Draw(color::BLUE);
        test11.Draw(color::DARK_GREY);
        test12.Draw(color::GOLD);
        test13.Draw(color::BROWN, true);
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
    Tests game;
    game.Construct("Tests", { 800, 800 });
    return 0;
}