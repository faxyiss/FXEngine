#pragma once

#include "FXEngine/Events/Event.h"

namespace FX
{
    class WindowCloseEvent : public Event
    {
    public:
        FX_EVENT_CLASS_TYPE(WindowClose)
    };

    class WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(std::uint32_t width, std::uint32_t height)
            : m_Width(width), m_Height(height) {}

        std::uint32_t GetWidth() const { return m_Width; }
        std::uint32_t GetHeight() const { return m_Height; }

        FX_EVENT_CLASS_TYPE(WindowResize)

    private:
        std::uint32_t m_Width, m_Height;
    };
}
