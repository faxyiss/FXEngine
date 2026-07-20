#pragma once

#include "FXEngine/Events/Event.h"

namespace FX
{
    // Modifier durumu olayin YANINDA tasiniyor, ayrica Input'a
    // sorulmuyor: olay islendiginde tuslar birakilmis olabilir.
    struct KeyModifiers
    {
        bool Ctrl  = false;
        bool Shift = false;
        bool Alt   = false;
    };

    class KeyPressedEvent : public Event
    {
    public:
        KeyPressedEvent(Key key, KeyModifiers mods, bool repeat)
            : m_Key(key), m_Mods(mods), m_Repeat(repeat) {}

        Key          GetKey() const { return m_Key; }
        KeyModifiers GetMods() const { return m_Mods; }

        // Tus BASILI TUTULDUGU icin tekrar eden olay. Kisayollar bunu
        // yok sayar (Ctrl+S'i basili tutmak on kez kaydetmemeli),
        // metin girisi saymak ister.
        bool IsRepeat() const { return m_Repeat; }

        FX_EVENT_CLASS_TYPE(KeyPressed)

    private:
        Key          m_Key;
        KeyModifiers m_Mods;
        bool         m_Repeat;
    };

    class KeyReleasedEvent : public Event
    {
    public:
        explicit KeyReleasedEvent(Key key) : m_Key(key) {}

        Key GetKey() const { return m_Key; }

        FX_EVENT_CLASS_TYPE(KeyReleased)

    private:
        Key m_Key;
    };
}
