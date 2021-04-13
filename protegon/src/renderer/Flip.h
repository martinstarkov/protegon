#pragma once 

namespace engine {

class Flip {
public:
    static constexpr int NONE{ 0 }; // SDL_FLIP_NONE
    static constexpr int HORIZONTAL{ 1 }; // SDL_FLIP_HORIZONTAL
    static constexpr int VERTICAL{ 2 }; // SDL_FLIP_VERTICAL
    static constexpr int BOTH{ 3 }; // SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL

    Flip() = default;
    Flip(int direction) : direction{ direction } {}
    operator int() const {
        return direction;
    }
private:
    int direction{ NONE };
};

} // namespace engine