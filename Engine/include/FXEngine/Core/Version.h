#pragma once

// ---------------------------------------------------------------------------
// Faz 0'in tek amaci: derleme zincirinin ucundan ucuna calistigini kanitlamak.
// Editor -> FXEngine link'i gercekten kuruluyor mu? Bu fonksiyon onu gosterir.
// Ilerideki fazlarda burasi gercek motor API'siyle dolacak.
// ---------------------------------------------------------------------------

namespace FX
{
    // const char* donduruyoruz, std::string degil: header'a <string> sokmadan
    // is goruyoruz. Motor header'larini hafif tutmak derleme suresini dusurur.
    const char* GetEngineVersion();
}
