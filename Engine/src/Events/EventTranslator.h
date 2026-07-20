#pragma once

// ===========================================================================
// SDL olaylarini motor olaylarina cevirir. Engine'e OZEL (src/ altinda):
// motor kullanicisinin SDL_Event gormesi gerekmiyor.
//
// std::unique_ptr donuyoruz cunku olay POLIMORFIK ve omru tek bir cagri
// boyunca. Havuz (pool) kullanmak erken optimizasyon olurdu - kare basina
// birkac olay dusuyor.
// ===========================================================================

#include "FXEngine/Events/Event.h"

#include <memory>

union SDL_Event;

namespace FX::Detail
{
    // Ilgilenmedigimiz SDL olaylari icin nullptr doner.
    std::unique_ptr<Event> TranslateSDLEvent(const SDL_Event& event);
}
