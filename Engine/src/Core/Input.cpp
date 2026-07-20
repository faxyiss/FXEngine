#include "FXEngine/Core/Input.h"

#include <SDL3/SDL.h>

namespace FX
{
    bool Input::IsKeyPressed(Key key)
    {
        // SDL'in klavye dizisi scancode ile INDEKSLENIR ve Key
        // degerlerimiz zaten scancode; cast yeterli.
        const bool* keys = SDL_GetKeyboardState(nullptr);
        if (!keys)
            return false;

        return keys[static_cast<int>(key)];
    }

    bool Input::IsMouseButtonPressed(MouseButton button)
    {
        const SDL_MouseButtonFlags state = SDL_GetMouseState(nullptr, nullptr);
        return (state & SDL_BUTTON_MASK(static_cast<int>(button))) != 0;
    }

    void Input::GetMousePosition(float& x, float& y)
    {
        SDL_GetMouseState(&x, &y);
    }

    float Input::GetMouseX()
    {
        float x = 0.0f, y = 0.0f;
        GetMousePosition(x, y);
        return x;
    }

    float Input::GetMouseY()
    {
        float x = 0.0f, y = 0.0f;
        GetMousePosition(x, y);
        return y;
    }
}
