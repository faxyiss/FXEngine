#pragma once

// ===========================================================================
// FXEngine - Event
//
// Motorun kendi olay tipleri. SDL_Event bir union'dir ve SDL'e bagimlidir;
// bunlar sinif hiyerarsisidir ve motora aittir.
//
// NEDEN SANAL FONKSIYON, std::variant DEGIL? Variant daha hizli olurdu ama
// yeni bir olay tipi eklemek TUM ziyaretcileri degistirmeyi gerektirirdi.
// Olay tipleri zamanla artar; kare basina birkac dusen bu maliyet
// olculebilir degil.
//
// Handled bayragi: bir olayi TUKETEN katman digerlerinin gormesini
// engeller. Editorde ImGui bir metin kutusuna yaziyorsa, ayni tuslar
// viewport kisayolu olarak da calismamali.
// ===========================================================================

#include "FXEngine/Core/KeyCodes.h"

#include <cstdint>

namespace FX
{
    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResize,
        KeyPressed,
        KeyReleased,
        MouseMoved,
        MouseScrolled,
        MouseButtonPressed,
        MouseButtonReleased
    };

    class Event
    {
    public:
        virtual ~Event() = default;

        virtual EventType GetType() const = 0;

        // Hata ayiklama ve log icin; olay akisini izlemek yazili bir
        // isim olmadan cok zor.
        virtual const char* GetName() const = 0;

        bool Handled = false;
    };

    // Tek bir olayi tipine gore ele almak icin. Sanal fonksiyon yerine
    // sablon kullaniyoruz: cagiran lambda yaziyor, olay tipi derleme
    // zamaninda biliniyor.
    //
    //   EventDispatcher d(e);
    //   d.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& k) { ... });
    //
    // Lambda true donerse olay TUKETILMIS sayilir.
    class EventDispatcher
    {
    public:
        explicit EventDispatcher(Event& event) : m_Event(event) {}

        template<typename T, typename F>
        bool Dispatch(const F& func)
        {
            if (m_Event.Handled || m_Event.GetType() != T::GetStaticType())
                return false;

            m_Event.Handled = func(static_cast<T&>(m_Event));
            return true;
        }

    private:
        Event& m_Event;
    };
}

// Her olay sinifinda tekrar eden uc satiri tek yerde tutuyoruz.
// EventDispatcher'in derleme zamaninda tip karsilastirabilmesi icin
// hem statik hem sanal bir tip sorgusu gerekiyor.
#define FX_EVENT_CLASS_TYPE(type)                                    \
    static EventType GetStaticType() { return EventType::type; }     \
    EventType   GetType() const override { return GetStaticType(); } \
    const char* GetName() const override { return #type; }
