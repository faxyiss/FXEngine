#include "Events/EventTranslator.h"

#include "FXEngine/Events/KeyEvent.h"
#include "FXEngine/Events/MouseEvent.h"
#include "FXEngine/Events/WindowEvent.h"

#include <SDL3/SDL.h>

namespace FX::Detail
{
    namespace
    {
        KeyModifiers ModsFrom(SDL_Keymod mod)
        {
            KeyModifiers mods;
            mods.Ctrl  = (mod & SDL_KMOD_CTRL)  != 0;
            mods.Shift = (mod & SDL_KMOD_SHIFT) != 0;
            mods.Alt   = (mod & SDL_KMOD_ALT)   != 0;
            return mods;
        }

        // Key degerleri scancode ile ayni oldugu icin ceviri bir cast.
        // Tanimadigimiz tuslar Unknown DEGIL kendi sayilariyla geciyor:
        // enum'da adi olmayan bir tusu de kisayol yapabilmek mumkun kalsin.
        Key KeyFrom(SDL_Scancode scancode)
        {
            return static_cast<Key>(scancode);
        }
    }

    std::unique_ptr<Event> TranslateSDLEvent(const SDL_Event& event)
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                return std::make_unique<WindowCloseEvent>();

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                return std::make_unique<WindowResizeEvent>(
                    static_cast<std::uint32_t>(event.window.data1),
                    static_cast<std::uint32_t>(event.window.data2));

            case SDL_EVENT_KEY_DOWN:
                return std::make_unique<KeyPressedEvent>(
                    KeyFrom(event.key.scancode),
                    ModsFrom(event.key.mod),
                    event.key.repeat);

            case SDL_EVENT_KEY_UP:
                return std::make_unique<KeyReleasedEvent>(KeyFrom(event.key.scancode));

            case SDL_EVENT_MOUSE_MOTION:
                return std::make_unique<MouseMovedEvent>(event.motion.x, event.motion.y);

            case SDL_EVENT_MOUSE_WHEEL:
                return std::make_unique<MouseScrolledEvent>(
                    event.wheel.x, event.wheel.y,
                    event.wheel.mouse_x, event.wheel.mouse_y);

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                return std::make_unique<MouseButtonPressedEvent>(
                    static_cast<MouseButton>(event.button.button),
                    event.button.x, event.button.y);

            case SDL_EVENT_MOUSE_BUTTON_UP:
                return std::make_unique<MouseButtonReleasedEvent>(
                    static_cast<MouseButton>(event.button.button),
                    event.button.x, event.button.y);

            default:
                return nullptr;
        }
    }
}
