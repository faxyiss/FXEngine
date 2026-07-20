#pragma once

#include "FXEngine/Events/Event.h"

namespace FX
{
    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}

        float GetX() const { return m_X; }
        float GetY() const { return m_Y; }

        FX_EVENT_CLASS_TYPE(MouseMoved)

    private:
        float m_X, m_Y;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        // x/y: kaydirma MIKTARI. mouseX/mouseY: olay anindaki imlec
        // konumu - imlece dogru zoom bunu istiyor ve olaydan sonra
        // fare kimildarsa Input'a sormak yanlis cevap verir.
        MouseScrolledEvent(float xOffset, float yOffset, float mouseX, float mouseY)
            : m_XOffset(xOffset), m_YOffset(yOffset), m_MouseX(mouseX), m_MouseY(mouseY) {}

        float GetXOffset() const { return m_XOffset; }
        float GetYOffset() const { return m_YOffset; }
        float GetMouseX() const { return m_MouseX; }
        float GetMouseY() const { return m_MouseY; }

        FX_EVENT_CLASS_TYPE(MouseScrolled)

    private:
        float m_XOffset, m_YOffset;
        float m_MouseX, m_MouseY;
    };

    class MouseButtonPressedEvent : public Event
    {
    public:
        MouseButtonPressedEvent(MouseButton button, float x, float y)
            : m_Button(button), m_X(x), m_Y(y) {}

        MouseButton GetButton() const { return m_Button; }
        float GetX() const { return m_X; }
        float GetY() const { return m_Y; }

        FX_EVENT_CLASS_TYPE(MouseButtonPressed)

    private:
        MouseButton m_Button;
        float       m_X, m_Y;
    };

    class MouseButtonReleasedEvent : public Event
    {
    public:
        MouseButtonReleasedEvent(MouseButton button, float x, float y)
            : m_Button(button), m_X(x), m_Y(y) {}

        MouseButton GetButton() const { return m_Button; }
        float GetX() const { return m_X; }
        float GetY() const { return m_Y; }

        FX_EVENT_CLASS_TYPE(MouseButtonReleased)

    private:
        MouseButton m_Button;
        float       m_X, m_Y;
    };
}
